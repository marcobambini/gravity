//
//  gravity_http.h
//  gravity
//
//  Created by Marco Bambini on 14/08/2017.
//  Copyright © 2017 CreoLabs. All rights reserved.
//

#ifndef __GRAVITY_HTTP__
#define __GRAVITY_HTTP__

#include "gravity_value.h"


typedef struct {
    const char *name; // name of header; e.g. Last-Modified
    const char *value; // value of header
} Header;

typedef struct {
    Header *headers;
    char *body;
    char *hostname;
    int status_code;
    char *status_message;
    int headercount;
} Response;

typedef struct {
    char *body;
    char *scheme;
    char *hostname;
    char *path;
    int   port;
    char *method;
    gravity_map_t *data;
    bool  use_ssl;
    int   fd;
    void  *conn;
    void *ctx;
} Request;

// http library
static Request *http_request_new(gravity_vm *vm, char *hostname, char *path, int port, char *method, gravity_map_t *data);
static void http_request_ssl_init_ctx(Request *req);
static void http_request_connect_ssl(Request *req);
static struct addrinfo *http_get_host_info(char *host, int port);
static void http_request_connect_tcp(Request *req);
static Response *http_request_connect(gravity_vm *vm, char *hostname, char *path, int port, char *method, gravity_map_t *data);
static Response *http_response_new(gravity_vm *vm, Request *req);
static void http_response_free(Response *resp);
static void http_response_parse_header(Response *resp, char *header);
static int8_t http_response_parse_line(Response *resp, char *line);
static void http_response_slurp_body(Response *resp, char **bk);
static void http_response_parse(Response *resp, char *source);
static ssize_t http_request_send(gravity_vm *vm, Request *req);
static Response *http_response_receive(gravity_vm *vm, Request *req);
static bool http_request (gravity_vm *vm, gravity_map_t *options, uint32_t rindex);
static bool http_get (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex);
static bool http_post (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex);


// gravity bindings
void gravity_http_register (gravity_vm *vm);
void gravity_http_free (void);
bool gravity_ishttp_class (gravity_class_t *c);
const char *gravity_http_name (void);

#endif
