/*
 * libws.h -- tiny websocket library, need openssl.
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

/**
 *
 * http://www.rfc-editor.org/rfc/rfc6455.txt
 *
 *     0                   1                   2                   3
 *     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *    +-+-+-+-+-------+-+-------------+-------------------------------+
 *    |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
 *    |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
 *    |N|V|V|V|       |S|             |   (if payload len==126/127)   |
 *    | |1|2|3|       |K|             |                               |
 *    +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
 *    |     Extended payload length continued, if payload len == 127  |
 *    + - - - - - - - - - - - - - - - +-------------------------------+
 *    |                               |Masking-key, if MASK set to 1  |
 *    +-------------------------------+-------------------------------+
 *    | Masking-key (continued)       |          Payload Data         |
 *    +-------------------------------- - - - - - - - - - - - - - - - +
 *    :                     Payload Data continued ...                :
 *    + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
 *    |                     Payload Data continued ...                |
 *    +---------------------------------------------------------------+
 *
 *
 */

#ifndef _LIBWS_H_
#define _LIBWS_H_

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__) && (__GNUC__ >= 4)
# define LIBWS_API __attribute__((visibility("default")))
#else
# define LIBWS_API
#endif

#include <stdint.h>
#include <stddef.h>

#define WS_KEY_LEN          24
#define WS_ACCEPT_LEN       28
#define WS_SECRET_LEN       36

#define WS_MASK             13
#define WS_SECRET           "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"


/** Websocket frame opcode. */
#define WS_OPCODE_CONTINUATION  0x0
#define WS_OPCODE_TEXT          0x1
#define WS_OPCODE_BINARY        0x2
#define WS_OPCODE_CLOSE         0x8
#define WS_OPCODE_PING          0x9
#define WS_OPCODE_PONG          0xa

/** Websocket frame flag.*/
#define WS_FLAG_FIN             0x10
#define WS_FLAG_MASK            0x20


/** Websocket close frame status. */
#define WS_STATUS_NORMAL                1000
#define WS_STATUS_GOING_AWAY            1001
#define WS_STATUS_PROTOCOL_ERROR        1002
#define WS_STATUS_UNSUPPORTED_DATA_TYPE 1003
#define WS_STATUS_STATUS_NOT_AVAILABLE  1005
#define WS_STATUS_ABNORMAL_CLOSED       1006
#define WS_STATUS_INVALID_PAYLOAD       1007
#define WS_STATUS_POLICY_VIOLATION      1008
#define WS_STATUS_MESSAGE_TOO_BIG       1009
#define WS_STATUS_INVALID_EXTENSION     1010
#define WS_STATUS_UNEXPECTED_CONDITION  1011
#define WS_STATUS_TLS_HANDSHAKE_ERROR   1015

/** Websocket request and response header flag. */
#define WS_HEADER_VERSION           0x01
#define WS_HEADER_UPGRADE           0x02
#define WS_HEADER_CONNECTION        0x04
#define WS_HEADER_KEY               0x08
#define WS_HEADER_ACCEPT            0x10
#define WS_HEADER_PROTOCOL          0x20

#define WS_HEADER_REQ           (WS_HEADER_VERSION | \
                                 WS_HEADER_UPGRADE | \
                                 WS_HEADER_CONNECTION | \
                                 WS_HEADER_KEY)

#define WS_HEADER_RSP           (WS_HEADER_UPGRADE | \
                                 WS_HEADER_CONNECTION | \
                                 WS_HEADER_ACCEPT)

//help define
#define WS_CLOSE_STATUS(payload)    ((((payload).data[0]) << 8) | (unsigned char)((payload).data[1]))
#define WS_CLOSE_REASON(payload)    ((payload).data + 2)
#define WS_CLOSE_REASON_LEN(payload)    ((int)(payload).length - 2)
#define WS_CLOSE_FRAME(payload, status, reason) \
    do {payload[0] = (char)(status >> 8); payload[1] = (char)(status & 0xff); memcpy(payload+2, reason, strlen(reason));} while (0)

#define WS_SWAP16(s)        ((((s) & 0xff) << 8) | (((s) >> 8) & 0xff))
#define WS_SWAP32(l)        (((l) >> 24) | (((l) & 0x00ff0000) >> 8) | (((l) & 0x0000ff00) << 8) | ((l) << 24))
#define WS_SWAP64(ll)       (((ll) >> 56) |\
                             (((ll) & 0x00ff000000000000) >> 40) | \
                             (((ll) & 0x0000ff0000000000) >> 24) | \
                             (((ll) & 0x000000ff00000000) >> 8) | \
                             (((ll) & 0x00000000ff000000) << 8) | \
                             (((ll) & 0x0000000000ff0000) << 24) | \
                             (((ll) & 0x000000000000ff00) << 40) | \
                             (((ll) << 56)))

#define WS_BUILD_OPCODE(flags, op)  (flags |= op)
#define WS_BUILD_FIN(flags)         (flags |= WS_FLAG_FIN)
#define WS_BUILD_MASK(flags)        (flags |= WS_FLAG_MASK)

struct libws_b {
    char *data;
    uint64_t length;
};

struct libws_frame {
    int opcode;
    int fin;
    int mask;
    struct libws_b payload;
};

struct libws_parser {
    int state;
    uint64_t require;

    char mask[4];
    int flags;
    uint64_t mask_offset;
    uint64_t offset;
    uint64_t length;
};

/**
 * calculate a websocket frame buff size with length payload
 * when a frame from c to s, mask = 1
 */
extern LIBWS_API uint64_t libws__build_size(int mask, uint64_t length);

/**
 * build a websocket frame into data
 * 
 * flags:
 *      opcode - WS_BUILD_OPCODE()
 *         fin - WS_BUILD_FIN()
 *        mask - WS_BUILD_MASK()
 */
extern LIBWS_API void libws__build(char *data, int flags, struct libws_b *payload);

/**
 * initialize a websocket frame parser
 */
extern LIBWS_API void libws__parser_init(struct libws_parser *p);

/**
 * parse a websocket frame from b
 * 
 * return:
 *      -1 - parse error
 *       0 - parse finish, need more data
 *       1 - a websocket frame parsed
 */
extern LIBWS_API int libws__parser_execute(struct libws_parser *p, struct libws_b *b, struct libws_frame *f);

/**
 * initialize a websocket request
 */
extern LIBWS_API int libws__request(char *request, size_t len, const char *url, const char *host, const char *origin,
                                    const char *protocol, char key[WS_KEY_LEN]);

/**
 * response a websocket resquest
 */
extern LIBWS_API int libws__response(char *response, size_t len, const char *server, const char *protocol,
                                     const char key[WS_KEY_LEN], char accept[WS_ACCEPT_LEN]);

/**
 * check http request and response header for websocket
 * when request flags == WS_HEADER_REQ, when response flags == WS_HEADER_RSP
 * return header flag
 */
extern LIBWS_API int libws__valid_header(int *flags, const char *key, size_t key_len, const char *value, size_t value_len);

/**
 * generate a websocket request key
 */
extern LIBWS_API void libws__generate_key(char key[WS_KEY_LEN]);

/**
 * generate a websocket accept for response
 */
extern LIBWS_API void libws__generate_accept(char accept[WS_ACCEPT_LEN], const char key[WS_KEY_LEN]);

/**
 * check accept and key when handshake
 */
extern LIBWS_API int libws__handshake(const char key[WS_KEY_LEN], const char accept[WS_ACCEPT_LEN]);

#ifdef __cplusplus
}
#endif

#endif // _LIBWS_H_

#ifdef LIBWS_IMPLEMENTATION

/**
 * Implement
 */

#include <openssl/sha.h>
#include <openssl/evp.h>

#include <string.h>
#include <assert.h>
#include <ctype.h>


enum libws_state {
    s_start = 0,
    s_head,
    s_length,
    s_mask,
    s_body,
};


static const unsigned char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char *
strncasestr(const char *haystack, const char *needle, size_t len) {
    char c, sc;
    size_t slen;

    if ((c = *needle++) != '\0') {
        slen = strlen(needle);
        do {
            do {
                if (len-- < 1 || (sc = *haystack++) == '\0')
                    return 0;
            } while (tolower(sc) != tolower(c));
            if (slen > len)
                return 0;
        } while (strncasecmp(haystack, needle, slen) != 0);
        haystack--;
    }
    return (char *)haystack;
}

static uint64_t
frame_mask(char *buff, char mask[4], const char *payload, uint64_t length, uint64_t offset) {
    uint64_t i;
    for (i = 0; i < length; i++)
        buff[i] = payload[i] ^ mask[(i + offset) % 4];
    return ((i + offset) % 4);
}

uint64_t
libws__build_size(int mask, uint64_t length) {
    return 2 + length + (mask ? 4 : 0) + (length >= 0x7e ? (length > 0xffff ? 8 : 2) : 0);
}

void
libws__build(char *data, int flags, struct libws_b *payload) {
    int offset;
    uint32_t mask = WS_MASK;
    uint64_t length = payload->length;

    data[0] = 0;
    data[1] = 0;
    if (flags & WS_FLAG_FIN) data[0] = (char)(1 << 7);
    data[0] |= (char)(flags & 0xf);
    if (flags & WS_FLAG_MASK) data[1] = (char)(1 << 7);
    if (length < 0x7e) {
        data[1] |= (char)length;
        offset = 2;
    } else if (length <= 0xffff) {
        data[1] |= 0x7e;
        data[2] = (char)(length >> 8);
        data[3] = (char)(length & 0xff);
        offset = 4;
    } else {
        data[1] |= 0x7f;
        data[2] = (char)((length >> 56) & 0xff);
        data[3] = (char)((length >> 48) & 0xff);
        data[4] = (char)((length >> 40) & 0xff);
        data[5] = (char)((length >> 32) & 0xff);
        data[6] = (char)((length >> 24) & 0xff);
        data[7] = (char)((length >> 16) & 0xff);
        data[8] = (char)((length >>  8) & 0xff);
        data[9] = (char)((length >>  0) & 0xff);
        offset = 10;
    }
    if (flags & WS_FLAG_MASK) {
        memcpy(&data[offset], &mask, 4);
        offset += 4;
        if (payload->data && length)
            frame_mask(&data[offset], (char *)&mask, payload->data, length, 0);
    } else if (payload->data && length)
        memcpy(&data[offset], payload->data, length);
}

void
libws__parser_init(struct libws_parser *p) {
    memset(p, 0, sizeof *p);
    p->state = s_start;
}

int
libws__parser_execute(struct libws_parser *p, struct libws_b *b, struct libws_frame *f) {
    char *s = b->data;
    char *e = b->data + b->length;
    long offset = 0;

    while (s < e) {
        switch(p->state) {
        case s_start:
            p->offset = 0;
            p->length = 0;
            p->mask_offset = 0;
            p->flags = ((*s) & 0xf);
            if ((*s) & (1 << 7))
                p->flags |= WS_FLAG_FIN;
            p->state = s_head;
            offset++;
            s++;
            break;
        case s_head:
            p->length  = (*s) & 0x7f;
            if ((*s) & 0x80)
                p->flags |= WS_FLAG_MASK;
            if (p->length >= 0x7e) {
                if (p->length == 0x7f)
                    p->require = 8;
                else
                    p->require = 2;
                p->length = 0;
                p->state = s_length;
            } else if (p->flags & WS_FLAG_MASK) {
                p->state = s_mask;
                p->require = 4;
            } else if (p->length) {
                p->state = s_body;
                p->require = p->length;
                f->opcode = p->flags & 0xf;
                f->fin = !!(p->flags & 0x10);
                f->mask = !!(p->flags & 0x20);
                f->payload.length = p->length;
                f->payload.data = malloc(p->length);
            } else {
                p->state = s_start;
                f->opcode = p->flags & 0xf;
                f->fin = !!(p->flags & 0x10);
                f->mask = !!(p->flags & 0x20);
                f->payload.length = p->length;
                f->payload.data = 0;
                b->length = e - s;
                b->data = s;
                return 1;
            }
            offset++;
            s++;
            break;
        case s_length:
            while(s < e && p->require) {
                p->length <<= 8;
                p->length |= (unsigned char)(*s);
                p->require--;
                offset++;
                s++;
            }
            if (!p->require) {
                if (p->flags & WS_FLAG_MASK) {
                    p->state = s_mask;
                    p->require = 4;
                } else if (p->length) {
                    p->state = s_body;
                    p->require = p->length;
                    f->opcode = p->flags & 0xf;
                    f->fin = !!(p->flags & 0x10);
                    f->mask = !!(p->flags & 0x20);
                    f->payload.length = p->length;
                    f->payload.data = malloc(p->length);
                } else {
                    p->state = s_start;
                    f->opcode = p->flags & 0xf;
                    f->fin = !!(p->flags & 0x10);
                    f->mask = !!(p->flags & 0x20);
                    f->payload.length = p->length;
                    f->payload.data = 0;
                    b->length = e - s;
                    b->data = s;
                    return 1;
                }
            }
            break;
        case s_mask:
            while(s < e && p->require) {
                p->mask[4 - p->require--] = *s;
                offset++;
                s++;
            }
            if (!p->require) {
                if (p->length) {
                    p->state = s_body;
                    p->require = p->length;
                    f->opcode = p->flags & 0xf;
                    f->fin = !!(p->flags & 0x10);
                    f->mask = !!(p->flags & 0x20);
                    f->payload.length = p->length;
                    f->payload.data = malloc(p->length);
                } else {
                    p->state = s_start;
                    f->opcode = p->flags & 0xf;
                    f->fin = !!(p->flags & 0x10);
                    f->mask = !!(p->flags & 0x20);
                    f->payload.length = p->length;
                    f->payload.data = 0;
                    b->length = e - s;
                    b->data = s;
                    return 1;
                }
            }
            break;
        case s_body:
            if (p->require) {
                if (s + p->require <= e) {
                    p->mask_offset = frame_mask(f->payload.data + p->offset, p->mask, s, p->require, p->mask_offset);
                    s += p->require;
                    p->require = 0;
                    offset = s - b->data;
                } else {
                    p->mask_offset = frame_mask(f->payload.data + p->offset, p->mask, s, (uint64_t)(e - s), p->mask_offset);
                    p->require -= (uint64_t)(e - s);
                    s = e;
                    p->offset += (uint64_t)(s - b->data - offset);
                    offset = 0;
                }
            }
            if (!p->require) {
                p->state = s_start;
                f->opcode = p->flags & 0xf;
                f->fin = !!(p->flags & 0x10);
                f->mask = !!(p->flags & 0x20);
                f->payload.length = p->length;
                b->length = e - s;
                b->data = s;
                return 1;
            }
            break;
        }
    }
    return 0;
}

void
libws__generate_key(char key[WS_KEY_LEN]) {
    unsigned char randkey[16];
    unsigned char _key[WS_KEY_LEN+1];
    int i, n;

    for (i = 0; i < 16; i++) {
        randkey[i] = b64[(rand() + time(0)) % 61];
    }
    n = EVP_EncodeBlock(_key, randkey, 16);
    assert(n == WS_KEY_LEN);
    strncpy(key, (char *)_key, WS_KEY_LEN);
}

void
libws__generate_accept(char accept[WS_ACCEPT_LEN], const char key[WS_KEY_LEN]) {
    int n;
    unsigned char buff[WS_KEY_LEN + WS_SECRET_LEN];
    unsigned char _accept[WS_ACCEPT_LEN+1];
    unsigned char *digest;

    memcpy(buff, key, WS_KEY_LEN);
    memcpy(buff + WS_KEY_LEN, WS_SECRET, WS_SECRET_LEN);
    digest = SHA1(buff, (size_t)(WS_KEY_LEN + WS_SECRET_LEN), 0);
    n = EVP_EncodeBlock(_accept, digest, SHA_DIGEST_LENGTH);
    assert(n == WS_ACCEPT_LEN);
    strncpy(accept, (char *)_accept, WS_ACCEPT_LEN);
}

int
libws__request(char *request, size_t len, const char *url, const char *host, const char *origin,
                  const char *protocol, char key[WS_KEY_LEN]) {
    libws__generate_key(key);

    const char *fmt =
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Origin: %s\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %.*s\r\n"
        "Sec-WebSocket-Protocol: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";

    return snprintf(request, len, fmt, url, host, origin, WS_KEY_LEN, key, protocol);
}

int
libws__response(char *response, size_t len, const char *server, const char *protocol,
                   const char key[WS_KEY_LEN], char accept[WS_ACCEPT_LEN]) {

    libws__generate_accept(accept, key);

    const char *fmt =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Server: %s\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %.*s\r\n"
        "Sec-WebSocket-Protocol: %s\r\n"
        "\r\n";

    return snprintf(response, len, fmt, server, WS_ACCEPT_LEN, accept, protocol);
}

int
libws__handshake(const char key[WS_KEY_LEN], const char accept[WS_ACCEPT_LEN]) {
    char check_accept[WS_ACCEPT_LEN];
    libws__generate_accept(check_accept, key);
    return 0 == strncmp(check_accept, accept, WS_ACCEPT_LEN) ? 0 : -1;
}

int
libws__valid_header(int *flags, const char *key, size_t key_len, const char *value, size_t value_len) {
    if (!key || !value) return 0;

    if (0 == strncasecmp(key, "Sec-WebSocket-Version", key_len)) {
        if (0 == strncmp(value, "13", value_len)) {
            *flags |= WS_HEADER_VERSION;
            return WS_HEADER_VERSION;
        } else
            *flags &= ~WS_HEADER_VERSION;
    } else if (0 == strncasecmp(key, "Upgrade", key_len)) {
        if (0 == strncasecmp(value, "websocket", value_len)) {
            *flags |= WS_HEADER_UPGRADE;
            return WS_HEADER_UPGRADE;
        } else
            *flags &= ~WS_HEADER_UPGRADE;
    } else if (0 == strncasecmp(key, "Connection", key_len)) {
        if (strncasestr(value, "Upgrade", value_len)) {
            *flags |= WS_HEADER_CONNECTION;
            return WS_HEADER_CONNECTION;
        } else
            *flags &= ~WS_HEADER_CONNECTION;
    } else if (0 == strncasecmp(key, "Sec-WebSocket-Key", key_len)) {
        if (value_len == WS_KEY_LEN) {
            *flags |= WS_HEADER_KEY;
            return WS_HEADER_KEY;
        } else
            *flags &= ~WS_HEADER_KEY;
    } else if (0 == strncasecmp(key, "Sec-WebSocket-Accept", key_len)) {
        if (value_len == WS_ACCEPT_LEN) {
            *flags |= WS_HEADER_ACCEPT;
            return WS_HEADER_ACCEPT;
        } else
            *flags &= ~WS_HEADER_ACCEPT;
    } else if (0 == strncasecmp(key, "Sec-WebSocket-Protocol", key_len)) {
        return WS_HEADER_PROTOCOL;
    }
    return 0;
}

#endif /* LIBWS_IMPLEMENTATION */
