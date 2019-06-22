//
//  gravity_http.c
//  gravity
//
//  Created by Marco Bambini on 14/08/2017.
//  Copyright © 2017 CreoLabs. All rights reserved.
//
//  Based on: https://www.w3schools.com/jsref/jsref_obj_http.asp

/* Generic */
#include <time.h>
#include <inttypes.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
/* Network */
#include <netdb.h>
#include <sys/socket.h>
#include "gravity_vm.h"
#include "gravity_http.h"
#include "gravity_core.h"
#include "gravity_hash.h"
#include "gravity_utils.h"
#include "gravity_macros.h"
#include "gravity_vmmacros.h"

#ifdef GRAVITY_OPENSSL_ENABLED
#include <openssl/ssl.h>
#endif

#define HTTP_CLASS_NAME                  "Http"
#define HTTP_MIN_RESPONSE_BODY_SIZE      1024
#define HTTP_MAX_HEADERS_SIZE            60
#define HTTP_MAX_HOSTNAME_SIZE           60
#define HTTP_MAX_SCHEME_SIZE             25
#define HTTP_MAX_PORT_SIZE               6
#define HTTP_MAX_REQUEST_BODY_SIZE       1024 * 1024
#define HTTP_MAX_BUF_SIZE                1024 * 1024


static gravity_class_t              *gravity_class_http = NULL;
static uint32_t                     refcount = 0;

// MARK: - Implementation -

static Request *http_request_new(gravity_vm *vm, char *hostname, char *path, int port, char *method, gravity_map_t *data) {
    Request *req = mem_alloc(vm, sizeof(Request));
    if (strstr(hostname, "://") != NULL) {
        char *bk = hostname;
        char *scheme_prefix = strtok_r(hostname, "://", &bk);
        req->scheme = mem_alloc(NULL, strlen(scheme_prefix) + 3 * sizeof(char));
        sprintf(req->scheme, "%s://", scheme_prefix);
        req->hostname = strtok_r(NULL, "//", &bk);
    } else {
        req->scheme = "";
        req->hostname = hostname;
    }
    req->path = path;
    req->port = port;
    req->method = method;
    req->data = data;
    #ifdef GRAVITY_OPENSSL_ENABLED
    req->use_ssl = string_starts_with(req->scheme, "https://") || req->port == 443;
    #else
    req->use_ssl = false;
    #endif
    req->body = mem_alloc(NULL, HTTP_MAX_REQUEST_BODY_SIZE * sizeof(char));
    return req;
}

static void http_request_free(Request *req) {
    if (req != NULL)
        mem_free(req);
}

#ifdef GRAVITY_OPENSSL_ENABLED
static void http_request_ssl_fd(Request *req) {
    int fd;
    struct hostent *host;
    struct sockaddr_in addr;

    if ((host = gethostbyname(req->hostname)) == NULL) {
        perror(req->hostname);
        abort();
    }

    fd = socket(PF_INET, SOCK_STREAM, 0);
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(req->port);
    addr.sin_addr.s_addr = *(long*)(host->h_addr);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        close(fd);
        perror(req->hostname);
        abort();
    }

    req->fd = fd;
}

static void http_request_ssl_init_ctx(Request *req) {
    const SSL_METHOD *method;
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    method = SSLv23_client_method();
    if ((req->ctx = SSL_CTX_new(method)) == NULL) {
        perror("ssl_ctx");
        abort();
    }
}

static void http_request_connect_ssl(Request *req) {
    SSL_library_init();
    http_request_ssl_fd(req);
    http_request_ssl_init_ctx(req);
    if ((req->conn = SSL_new(req->ctx)) == NULL) {
        perror("Unable to create ssl connection");
        abort();
    }

    SSL_set_fd(req->conn, req->fd);
    if (SSL_connect(req->conn) != 1) {
        perror("connect_ssl");
    }
}
#endif

// Get host information (used to http_request_connect_tcp)
static struct addrinfo *http_request_host_info(char *host, int port) {
    int r;
    struct addrinfo hints, *getaddrinfo_res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    int length = (int)floor(log10((double)port)) + 1;
    char port_string[length];
    sprintf(port_string, "%d", port);
    if ((r = getaddrinfo(host, port_string, &hints, &getaddrinfo_res))) {
        fprintf(stderr, "[http_request_host_info:getaddrinfo] %s\n", gai_strerror(r));
        perror("getaddrinfo");
        return NULL;
    }
    return getaddrinfo_res;
}

static void http_request_connect_tcp(Request *req) {
    int fd;
    struct addrinfo *info = http_request_host_info(req->hostname, req->port);
    if (info == NULL) {
        perror("Unable to get host information");
        return;
    }

    for (;info != NULL; info = info->ai_next) {
        if ((fd = socket(info->ai_family,
            info->ai_socktype,
            info->ai_protocol)) < 0) {
            perror("Unable to obtain file descriptor");
            continue;
        }

        // HTTP
        if (connect(fd, info->ai_addr, info->ai_addrlen) < 0) {
            close(fd);
            perror("Unable to connect");
            continue;
        }

        freeaddrinfo(info);
        req->fd = fd;
        return;
    }
}

// Establish connection with host
static Response *http_request_connect(gravity_vm *vm, char *hostname, char *path, int port, char *method, gravity_map_t *data) {
    Request *req = http_request_new(NULL, hostname, path, port, method, data);
    #ifdef GRAVITY_OPENSSL_ENABLED
    if (req->use_ssl)
        http_request_connect_ssl(req);
    else
    #else
    if (true)
    #endif
        http_request_connect_tcp(req);

    if (req->fd == -1)
        perror("Failed to connect");

    http_request_send(vm, req);
    Response *resp = http_response_receive(vm, req);
    http_request_free(req);
    return resp;
}

static bool http_request_validate_options(gravity_vm *vm, gravity_map_t **options, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    if(!VALUE_ISA_MAP(args[1])) {
        RETURN_ERROR("Data must be a map.");
    }
    *options = VALUE_AS_MAP(args[1]);
    return true;
}

static bool http_request_validate_args(gravity_vm *vm, gravity_map_t **options, uint32_t rindex) {
    // validate host
    //  - required
    //  - must be a string
    gravity_value_t *host = gravity_hash_lookup_cstring((*options)->hash, "host");
    if (host == NULL)
        RETURN_ERROR("Host must be specified.");
    if (!VALUE_ISA_STRING(((gravity_value_t)(*host))))
        RETURN_ERROR("Host must be a string.");

    // validate path
    //  - not required, defaults to '/'
    //  - must be a string
    gravity_value_t *path = gravity_hash_lookup_cstring((*options)->hash, "path");
    if (path == NULL)
        gravity_map_insert(vm, *options, gravity_string_to_value(vm, "path", 4), gravity_string_to_value(vm, "/", 1));
    else if (!VALUE_ISA_STRING(((gravity_value_t)(*path))))
        RETURN_ERROR("Path must be a string.");

    // validate port
    //  - not required, defaults to "80"
    //  - must be a string
    gravity_value_t *port = gravity_hash_lookup_cstring((*options)->hash, "port");
    if (port == NULL)
        if (string_starts_with(VALUE_AS_CSTRING(*host), "https://"))
            gravity_map_insert(vm, *options, VALUE_FROM_CSTRING(vm, "port"), VALUE_FROM_INT(443));
        else
            gravity_map_insert(vm, *options, VALUE_FROM_CSTRING(vm, "port"), VALUE_FROM_INT(80));
    else if (!VALUE_ISA_INT(((gravity_value_t)(*port))))
        RETURN_ERROR("Port must be an integer.");

    // validate method
    //  - not required, defaults to "GET"
    //  - must be a string
    gravity_value_t *method = gravity_hash_lookup_cstring((*options)->hash, "method");
    if (method == NULL)
        gravity_map_insert(vm, *options, gravity_string_to_value(vm, "method", 6), gravity_string_to_value(vm, "GET", 3));
    else if (!VALUE_ISA_STRING(((gravity_value_t)(*method))))
        RETURN_ERROR("Method must be a string.");

    // validate data
    //  - not required, defaults to empty map
    //  - must be a map
    gravity_value_t *data = gravity_hash_lookup_cstring((*options)->hash, "data");
    if (data == NULL)
        gravity_map_insert(vm, *options, gravity_string_to_value(vm, "data", 4), gravity_map_to_value(vm, gravity_map_new(vm, 32)));
    else if (!VALUE_ISA_MAP(((gravity_value_t)(*data))))
        RETURN_ERROR("Data must be a map.");

    return true;
}

static Response *http_response_new(gravity_vm *vm, Request *req) {
    Response *resp = mem_alloc(vm, sizeof(Response));
    resp->headers = mem_alloc(vm, 60 * sizeof(Header)); // max of 60 headers; 48 standard, 12 non-standard
    resp->body = mem_alloc(vm, HTTP_MIN_RESPONSE_BODY_SIZE);
    resp->hostname = mem_alloc(vm, strlen(req->hostname));
    memcpy(resp->hostname, req->hostname, strlen(req->hostname) + 1);
    resp->headercount = 0;
    return resp;
}

static void http_response_free(Response *resp) {
    if (resp->headers != NULL)
        mem_free(resp->headers);
    if (resp != NULL)
        mem_free(resp);
}

static void http_response_parse_header(Response *resp, char *line) {
    char *bk = line;
    resp->headers[resp->headercount].name = strtok_r(line, ": ", &bk);
    resp->headers[resp->headercount++].value = strtok_r(NULL, "\r\n", &bk);
}

// Parses status_code and status_message; e.g. HTTP/1.0 403 Forbidden
static void http_response_parse_status(Response *resp, char *line) {
    char *bk = line;
    strtok_r(line, " ", &bk);
    resp->status_code = atoi(strtok_r(NULL, " ", &bk));
    resp->status_message = strtok_r(NULL, "\r\n", &bk);
}

static int8_t http_response_parse_line(Response *resp, char *line) {
    if (line == NULL)
        return 1;

    // get ready for body
    if (string_starts_with(line, "\r"))
        return 3;

    // check for opening HTTP response
    if (string_starts_with(line, "HTTP")) {
        http_response_parse_status(resp, line);
        return 0;
    }

    // check if header
    if (strstr(line, ":") != NULL) {
        http_response_parse_header(resp, line);
        return 0;
    }
    
    return 2;
}

static void http_response_slurp_body(Response *resp, char **bk) {
    char *token = NULL;
    while ((token = strtok_r(NULL, "\n", bk)) != NULL)
        strcat(resp->body, token);
}

static void http_response_parse(Response *resp, char *source) {
    char *token = NULL;
    char *bk = source;
    token = strtok_r(source, "\n", &bk);
    while (token) {
        int8_t status = http_response_parse_line(resp, token);
        if (status == 3)
            http_response_slurp_body(resp, &bk);
        token = strtok_r(NULL, "\n", &bk);
    }
}

static ssize_t http_request_send(gravity_vm *vm, Request *req) {
    sprintf(req->body, "%s %s HTTP/1.1\r\n", req->method, req->path);
    strcat(req->body, "Host: ");
    strcat(req->body, req->hostname);
    strcat(req->body, "\r\n");
    strcat(req->body, "User-Agent: Gravity\r\n");
    bool posting_data = strcmp(req->method, "POST") == 0;
    if (posting_data && req->data != NULL) {
        char *json = gravity_map_to_string(vm, req->data);
        char content_length_header[40];
        sprintf(content_length_header, "Content-Length: %lu\r\n", strlen(json));
        strcat(req->body, content_length_header);
        strcat(req->body, "Content-Type: application/json\r\n");
        strcat(req->body, "\r\n");
        strcat(req->body, json);
    } else {
        strcat(req->body, "Accept: */*\r\n");
        strcat(req->body, "Accept-Language: en-US,en;q=0.9\r\n");
    }

    strcat(req->body, "\r\n");
    ssize_t bytes;
    #ifdef GRAVITY_OPENSSL_ENABLED
    if (req->use_ssl) {
        bytes = SSL_write(req->conn, req->body, strlen(req->body));
    } else {
    #else
    if (true) {
    #endif
        bytes = send(req->fd, req->body, strlen(req->body), 0);
    }
    if (req->body != NULL)
        mem_free(req->body);
    return bytes;
}

static Response *http_response_receive(gravity_vm *vm, Request *req) {
    Response *resp = http_response_new(vm, req);
	char *buf = mem_alloc(vm, HTTP_MAX_BUF_SIZE * sizeof(char));

    #ifdef GRAVITY_OPENSSL_ENABLED
    if (req->use_ssl) {
        int bytes = SSL_read(req->conn, buf, HTTP_MAX_BUF_SIZE);
        if (bytes <= 0) {
            SSL_get_error(req->conn, bytes);
        }
    } else {
    #else
    if (true) {
    #endif
        while (recv(req->fd, buf, HTTP_MAX_BUF_SIZE, MSG_WAITSTREAM) > 0);
    }
    close(req->fd);
    #ifdef GRAVITY_OPENSSL_ENABLED
    SSL_CTX_free(req->ctx);
    #endif
    http_response_parse(resp, buf);
    mem_free(buf);
    return resp;
}

static bool http_request (gravity_vm *vm, gravity_map_t *options, uint32_t rindex) {
    bool valid_args = http_request_validate_args(vm, &options, rindex);
    if (!valid_args)
        return valid_args;

    int fd;
    char *hostname = VALUE_AS_CSTRING(*gravity_hash_lookup_cstring(options->hash, "host"));
    char *path = VALUE_AS_CSTRING(*gravity_hash_lookup_cstring(options->hash, "path"));
    int port = VALUE_AS_INT(*gravity_hash_lookup_cstring(options->hash, "port"));
    char *method = VALUE_AS_CSTRING(*gravity_hash_lookup_cstring(options->hash, "method"));
    gravity_map_t *data = VALUE_AS_MAP(*gravity_hash_lookup_cstring(options->hash, "data"));

    Response *resp = http_request_connect(vm, hostname, path, port, method, data);

    // build response map
    gravity_map_t *response = gravity_map_new(vm, 32);
    gravity_map_t *headers = gravity_map_new(vm, HTTP_MAX_HEADERS_SIZE);
    for (int i = 0; i < resp->headercount; i++)
        gravity_map_insert(vm, headers,
            VALUE_FROM_CSTRING(vm, resp->headers[i].name),
            VALUE_FROM_CSTRING(vm, resp->headers[i].value));
    gravity_map_insert(vm, response, VALUE_FROM_CSTRING(vm, "Headers"), VALUE_FROM_OBJECT(headers));
    gravity_map_insert(vm, response, VALUE_FROM_CSTRING(vm, "Body"), VALUE_FROM_CSTRING(vm, resp->body));
    gravity_map_insert(vm, response, VALUE_FROM_CSTRING(vm, "Hostname"), VALUE_FROM_CSTRING(vm, resp->hostname));
    gravity_map_insert(vm, response, VALUE_FROM_CSTRING(vm, "StatusCode"), VALUE_FROM_INT(resp->status_code));
    gravity_map_insert(vm, response, VALUE_FROM_CSTRING(vm, "StatusMessage"), VALUE_FROM_CSTRING(vm, resp->status_message));


    // free allocated
    http_response_free(resp);
    RETURN_VALUE(VALUE_FROM_OBJECT(response), rindex);
}

static bool http_get (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    gravity_map_t *options;
    bool valid_options = http_request_validate_options(vm, &options, args, nargs, rindex);
    if (!valid_options)
        return valid_options;
    gravity_map_insert(vm, options,
        gravity_string_to_value(vm, "method", strlen("method")),
        gravity_string_to_value(vm, "GET", strlen("GET")));
    return http_request(vm, options, rindex);
}

static bool http_post (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    gravity_map_t *options;
    bool valid_options = http_request_validate_options(vm, &options, args, nargs, rindex);
    if (!valid_options)
        return valid_options;
    gravity_map_insert(vm, options,
        gravity_string_to_value(vm, "method", strlen("method")),
        gravity_string_to_value(vm, "POST", strlen("POST")));
    return http_request(vm, options, rindex);
}

// MARK: - Internals -

static void create_optional_class (void) {
    gravity_class_http = gravity_class_new_pair(NULL, HTTP_CLASS_NAME, NULL, 0, 0);
    gravity_class_t *meta = gravity_class_get_meta(gravity_class_http);
    gravity_class_bind(meta, "get", NEW_CLOSURE_VALUE(http_get));
    gravity_class_bind(meta, "post", NEW_CLOSURE_VALUE(http_post));
    gravity_closure_t *closure = NULL;
    SETMETA_INITED(gravity_class_http);
}

// MARK: - Commons -

bool gravity_ishttp_class (gravity_class_t *c) {
    return (c == gravity_class_http);
}

const char *gravity_http_name (void) {
    return HTTP_CLASS_NAME;
}

void gravity_http_register (gravity_vm *vm) {
    if (!gravity_class_http) create_optional_class();
    ++refcount;

    if (!vm || gravity_vm_ismini(vm)) return;
    gravity_vm_setvalue(vm, HTTP_CLASS_NAME, VALUE_FROM_OBJECT(gravity_class_http));
}

void gravity_http_free (void) {
    if (!gravity_class_http) return;
    if (--refcount) return;

    gravity_class_t *meta = gravity_class_get_meta(gravity_class_http);
    gravity_class_free_core(NULL, meta);
    gravity_class_free_core(NULL, gravity_class_http);

    gravity_class_http = NULL;
}

