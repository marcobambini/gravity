//
//  gravity_http.c
//  gravity
//
//  Created by Marco Bambini on 14/08/2017.
//  Copyright © 2017 CreoLabs. All rights reserved.
//
//  Based on: https://www.w3schools.com/jsref/jsref_obj_http.asp

#include <time.h>
#include <inttypes.h>
/* Generic */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

#define HTTP_CLASS_NAME             "Http"

#if GRAVITY_ENABLE_DOUBLE
#define BUF_SIZE                         1024
#else
#define BUF_SIZE                         1024
#endif

static gravity_class_t              *gravity_class_http = NULL;
static uint32_t                     refcount = 0;

// MARK: - Implementation -

// Get host information (used to establish_connection)
static struct addrinfo *get_host_info(char* host, char* port) {
  int r;
  struct addrinfo hints, *getaddrinfo_res;
  // Setup hints
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  if ((r = getaddrinfo(host, port, &hints, &getaddrinfo_res))) {
    fprintf(stderr, "[get_host_info:getaddrinfo] %s\n", gai_strerror(r));
    return NULL;
  }

  return getaddrinfo_res;
}

// Establish connection with host
static int establish_connection(struct addrinfo *info) {
  if (info == NULL) return -1;

  int fd;
  for (;info != NULL; info = info->ai_next) {
    if ((fd = socket(info->ai_family,
                           info->ai_socktype,
                           info->ai_protocol)) < 0) {
      perror("[establish_connection:socket]");
      continue;
    }

    if (connect(fd, info->ai_addr, info->ai_addrlen) < 0) {
      close(fd);
      perror("[establish_connection:connect]");
      continue;
    }

    freeaddrinfo(info);
    return fd;
  }

  freeaddrinfo(info);
  return -1;
}

static char *set_request(char *method, char *path) {
    char *req = (char *)mem_alloc(NULL, 1000 * sizeof(char));
    sprintf(req, "%s %s HTTP/1.0\r\n\r\n", method, path);
    return req;
}

static ssize_t send_request(int fd, char *method, char *path) {
    char *req = set_request(method, path);
    ssize_t b = send(fd, req, strlen(req), 0);
    mem_free(req);
    return b;
}

static char *receive_request(int fd, char **buf) {
	*buf = (char *)mem_alloc(NULL, BUF_SIZE * sizeof(char));
    while (recv(fd, *buf, BUF_SIZE, MSG_WAITALL) > 0);
    return *buf;
}

static bool http_request (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)

    if(!VALUE_ISA_STRING(args[1])) {
        RETURN_ERROR("Hostname must be a string.");
    }
    if(!VALUE_ISA_STRING(args[2])) {
        RETURN_ERROR("Path must be a string.");
    }
    if(!VALUE_ISA_STRING(args[3])) {
        RETURN_ERROR("Port must be an string.");
    }
    if(!VALUE_ISA_STRING(args[4])) {
        RETURN_ERROR("Method must be an string.");
    }

    int fd;
    char *hostname = VALUE_AS_CSTRING(args[1]);
    char *path = VALUE_AS_CSTRING(args[2]);
    char *port = VALUE_AS_CSTRING(args[3]);
    char *method = VALUE_AS_CSTRING(args[4]);

	fd = establish_connection(get_host_info(hostname, port));
    if (fd == -1) {
        RETURN_ERROR("Failed to connect.");
    }

    char *buf;
    send_request(fd, method, path);
    receive_request(fd, &buf);
    close(fd);

    gravity_map_t *response = gravity_map_new(vm, 32);
    char *token = NULL;
    token = strtok(buf, "\n");

    // TODO: fix this ugliness
    char content_length[3];
    char http_version[5];
    char status_code[5];
    char status_message[31];
    char content_type[50];
    char charset[20];
    char date[50];
    // parse headers
    while (token) {
        printf("Current token: %s\n", token);
        if (sscanf(token, "HTTP/%s %s %s\r\n", http_version, status_code, status_message) > 0) {
            gravity_map_insert(vm, response,
                gravity_string_to_value(vm, "http_version", strlen("http_version")),
                gravity_string_to_value(vm, http_version, strlen(http_version)));
            gravity_map_insert(vm, response,
                gravity_string_to_value(vm, "status_code", strlen("status_code")),
                gravity_string_to_value(vm, status_code, strlen(status_code)));
            gravity_map_insert(vm, response,
                gravity_string_to_value(vm, "status_message", strlen("status_message")),
                gravity_string_to_value(vm, status_message, strlen(status_message)));
        }
        else if (sscanf(token, "Content-Type: %s %s\r\n", content_type, charset) > 0) {
            gravity_map_insert(vm, response,
                gravity_string_to_value(vm, "content_type", strlen("content_type")),
                gravity_string_to_value(vm, content_type, strlen(content_type)));
            gravity_map_insert(vm, response,
                gravity_string_to_value(vm, "charset", strlen("charset")),
                gravity_string_to_value(vm, charset, strlen(charset)));
        }
        else if (sscanf(token, "Content-Length: %s\r\n", content_length) > 0)
            gravity_map_insert(vm, response,
                gravity_string_to_value(vm, "content_length", strlen("content_length")),
                gravity_string_to_value(vm, content_length, strlen(content_length)));
        else if (sscanf(token, "Date: %29c\r\n", date) > 0)
            gravity_map_insert(vm, response,
                gravity_string_to_value(vm, "date", strlen("date")),
                gravity_string_to_value(vm, date, strlen(date)));
        else if (strncmp(token, "\r\n", strlen(token)) == 0) {
            gravity_map_insert(vm, response,
                gravity_string_to_value(vm, "body_reached", strlen("body_reached")),
                gravity_string_to_value(vm, "true", strlen("true")));
            token = strtok(NULL, "\n");
            break;
        }
        token = strtok(NULL, "\n");
    }

    // parse body
    char body[1000];
    while (token) {
        // TODO: make body dynamic list
        strcat(body, token);
        token = strtok(NULL, "\n");
    }
    gravity_map_insert(vm, response,
        gravity_string_to_value(vm, "body", (uint16_t)strlen("body")),
        gravity_string_to_value(vm, body, (uint16_t)strlen(body)));

    mem_free(buf);
    RETURN_VALUE(VALUE_FROM_OBJECT(response), rindex);
}

static bool http_get (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    gravity_value_t method_val = gravity_string_to_value(vm, "GET", 3);
    args[nargs++] = method_val;
    return http_request(vm, args, nargs, rindex);
}

// MARK: - Internals -

static void create_optional_class (void) {
    gravity_class_http = gravity_class_new_pair(NULL, HTTP_CLASS_NAME, NULL, 0, 0);
    gravity_class_t *meta = gravity_class_get_meta(gravity_class_http);
    gravity_class_bind(meta, "get", NEW_CLOSURE_VALUE(http_get));
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
    // computed_property_free(meta, "E", true);
    gravity_class_free_core(NULL, meta);
    gravity_class_free_core(NULL, gravity_class_http);

    gravity_class_http = NULL;
}

