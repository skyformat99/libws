/*
 * libwshttp.h -- websocket library, need libws.
 *
 * Copyright (c) zhoukk <izhoukk@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _LIBWSHTTP_H_
#define _LIBWSHTTP_H_

#include "libws.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__) && (__GNUC__ >= 4)
# define LIBWSHTTP_API __attribute__((visibility("default")))
#else
# define LIBWSHTTP_API
#endif

#define LIBWSHTTP_MAX_HTTP_LEN 4096
#define LIBWSHTTP_MAX_PROTOCOL_LEN 16
#define LIBWSHTTP_DEF_SERVER "libws"

#define LIBWSHTTP_OPEN 1
#define LIBWSHTTP_DATA 2
#define LIBWSHTTP_CLOSE 3

struct libwshttp_event {
    int event;
    struct libws_frame f;
};

struct libwshttp;

/**
 *
 */
extern LIBWSHTTP_API struct libwshttp *libwshttp__create(int issrv, void *io, int (*write)(void *io, const char *data, int size), void (*close)(void *io));

/**
 *
 *
 */
extern LIBWSHTTP_API void libwshttp__destroy(struct libwshttp *wh);

/**
 *
 */
extern LIBWSHTTP_API int libwshttp__feed(struct libwshttp *wh, struct libws_b *b, struct libwshttp_event *evt);

/**
 *
 *
 */
extern LIBWSHTTP_API int libwshttp__write(struct libwshttp *wh, int opcode, struct libws_b *payload);

/**
 *
 */
extern LIBWSHTTP_API void libwshttp__close(struct libwshttp *wh, int close_status, const char *reason);

#ifdef __cplusplus
}
#endif

#endif // _LIBWSHTTP_H_

#ifdef LIBWSHTTP_IMPLEMENTATION

/**
 * Implement
 */

#define LIBWS_IMPLEMENTATION
#include "libws.h"

#include "http_parser.h"

struct libwshttp {
    int handshake;
    http_parser http_p;
    struct libws_parser ws_p;
    char key[WS_KEY_LEN];
    char accept[WS_ACCEPT_LEN];
    char protocol[LIBWSHTTP_MAX_PROTOCOL_LEN];
    const char *header_at;
    size_t header_length;
    int flags;
    int issrv;
    void *io;
    int (*write)(void *, const char *, int);
    void (*close)(void *);
};


static int
__on_message_begin(http_parser *p) {
    (void)p;
    fprintf(stdout, "__on_message_begin\n");
    return 0;
}

static int
__on_url(http_parser *p, const char *at, size_t length) {
    (void)p;
    fprintf(stdout, "__on_url %.*s\n", (int)length, at);
    return 0;
}

static int
__on_status(http_parser *p, const char *at, size_t length) {
    (void)p;
    fprintf(stdout, "__on_status %.*s\n", (int)length, at);
    return 0;
}

static int
__on_header_field(http_parser *p, const char *at, size_t length) {
    struct libwshttp *wh = (struct libwshttp *)p->data;
    wh->header_at = at;
    wh->header_length = length;
    fprintf(stdout, "__on_header_field %.*s\n", (int)length, at);
    return 0;
}

static int
__on_header_value(http_parser *p, const char *at, size_t length) {
    struct libwshttp *wh = (struct libwshttp *)p->data;
    int flag = libws__valid_header(&wh->flags, wh->header_at, wh->header_length, at, length);
    if (flag == WS_HEADER_KEY) {
        strncpy(wh->key, at, length);
    } else if (flag == WS_HEADER_ACCEPT) {
        strncpy(wh->accept, at, length);
    } else if (flag == WS_HEADER_PROTOCOL) {
        int n = length < LIBWSHTTP_MAX_PROTOCOL_LEN ? length : LIBWSHTTP_MAX_PROTOCOL_LEN;
        strncpy(wh->protocol, at, n);
    }
    fprintf(stdout, "__on_header_value %.*s\n", (int)length, at);
    return 0;
}

static int
__on_headers_complete(http_parser *p) {
    struct libwshttp *wh = (struct libwshttp *)p->data;
    fprintf(stdout, "__on_headers_complete\n");
    if (wh->flags != (wh->issrv ? WS_HEADER_REQ : WS_HEADER_RSP)) {
        return -1;
    }
    return 0;
}

static int
__on_body(http_parser *p, const char *at, size_t length) {
    (void)p;
    fprintf(stdout, "__on_body %.*s\n", (int)length, at);
    return 0;
}

static int
__on_message_complete(http_parser *p) {
    struct libwshttp *wh = (struct libwshttp *)p->data;
    fprintf(stdout, "__on_message_complete\n");
    if (wh->issrv) {
        char response[LIBWSHTTP_MAX_HTTP_LEN];
        int n = libws__response(response, LIBWSHTTP_MAX_HTTP_LEN, LIBWSHTTP_DEF_SERVER, wh->protocol, wh->key, wh->accept);
        if (wh->write(wh->io, response, n)) {
            return -1;
        } else {
            wh->handshake = 1;
        }
    } else {
        if (!libws__handshake(wh->key, wh->accept)) {
            wh->handshake = 1;
        } else {
            return -1;
        }
    }
    return 0;
}

struct libwshttp *
libwshttp__create(int issrv, void *io, int (*write)(void *io, const char *data, int size), void (*close)(void *io)) {
    struct libwshttp *wh;

    wh = (struct libwshttp *)malloc(sizeof *wh);
    memset(wh, 0, sizeof *wh);

    wh->issrv = issrv;
    wh->io = io;
    wh->write = write;
    wh->close = close;
    http_parser_init(&wh->http_p, issrv ? HTTP_REQUEST : HTTP_RESPONSE);
    wh->http_p.data = wh;
    libws__parser_init(&wh->ws_p);

    return wh;
}

int
libwshttp__request(struct libwshttp *wh, const char *url, const char *host, const char *protocol) {
    char request[LIBWSHTTP_MAX_HTTP_LEN];
    int n = libws__request(request, LIBWSHTTP_MAX_HTTP_LEN, url, host, host, protocol, wh->key);
    return wh->write(wh->io, request, n);
}

int
libwshttp__feed(struct libwshttp *wh, struct libws_b *b, struct libwshttp_event *evt) {
    static http_parser_settings settings = {
        .on_message_begin = __on_message_begin,
        .on_url = __on_url,
        .on_status = __on_status,
        .on_header_field = __on_header_field,
        .on_header_value = __on_header_value,
        .on_headers_complete = __on_headers_complete,
        .on_body = __on_body,
        .on_message_complete = __on_message_complete
    };

    if (b->length == 0) return 0;

    if (!wh->handshake) {
        int parsed = http_parser_execute(&wh->http_p, &settings, b->data, b->length);
        if (wh->http_p.http_errno) {
            fprintf(stderr, "http_parser_execute: %s %s\n", http_errno_name(wh->http_p.http_errno), http_errno_description(wh->http_p.http_errno));
            return -1;
        }
        b->length -= parsed;
        b->data += parsed;
        if (wh->handshake) {
            evt->event = LIBWSHTTP_OPEN;
            return 1;
        }
        return 0;
    } else {
        int rc;

        rc = libws__parser_execute(&wh->ws_p, b, &evt->f);
        if (rc <= 0) {
            return rc;
        }
        if (evt->f.opcode == WS_OPCODE_PING) {
            struct libws_b dummy = {0, 0};
            libwshttp__write(wh, WS_OPCODE_PONG, &dummy);
            return 0;
        }
        if (evt->f.opcode == WS_OPCODE_CLOSE) {
            evt->event = LIBWSHTTP_CLOSE;
        } else {
            evt->event = LIBWSHTTP_DATA;
        }
        return rc;
    }
}

int
libwshttp__write(struct libwshttp *wh, int opcode, struct libws_b *payload) {
    char *data;
    uint64_t size;
    int flags = 0;
    int rc;

    size = libws__build_size(wh->issrv ? 0 : 1, payload->length);
    data = malloc(size);
    WS_BUILD_OPCODE(flags, opcode);
    WS_BUILD_FIN(flags);
    if (!wh->issrv) WS_BUILD_MASK(flags);
    libws__build(data, flags, payload);
    rc = wh->write(wh->io, data, size);
    free(data);
    return rc;
}

void
libwshttp__close(struct libwshttp *wh, int close_status, const char *reason) {
    struct libws_b b;
    int len = strlen(reason);
    char payload[2 + len];
    uint16_t s = (uint16_t)((close_status & 0xff00) >> 8 | (close_status & 0xff) << 8);
    char *p = (char *)&s;
    memcpy(payload, p, sizeof s);
    memcpy(payload + sizeof s, reason, len);
    b.data = payload;
    b.length = len + 2;

    libwshttp__write(wh, WS_OPCODE_CLOSE, &b);
    wh->close(wh->io);
}

void
libwshttp__destroy(struct libwshttp *wh) {
    free(wh);
}

#endif /* LIBWSHTTP_IMPLEMENTATION */
