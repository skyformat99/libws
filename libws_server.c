#define LIBWS_IMPLEMENTATION
#include "libws.h"

#include "http_parser.h"

#include "lib/ae.h"
#include "lib/anet.h"

#include <unistd.h>
#include <errno.h>

struct ae_io {
    int fd;
    int handshake;
    http_parser http_p;
    struct libws_parser ws_p;
    char key[WS_KEY_LEN];
    char accept[WS_ACCEPT_LEN];
    int flags;
    const char *header_at;
    size_t header_length;
    char *protocol;
};

static char *host = 0;
static int port = 8080;
static int debug = 0;
static int quiet = 0;

static char *server = 0;

static void
usage(void) {
    printf("libws_server is a simple websocket server.\n");
    printf("libws_server version %s running on libws %d.%d.%d.\n\n", "0.0.0", 0, 2, 0);
    printf("Usage: libws_server [-h host] [-p port] [-s server]\n");
    printf("                     [-d] [--quiet]\n");
    printf("       libws_server --help\n\n");
    printf(" -d : enable debug messages.\n");
    printf(" -h : http host to connect to. Defaults to localhost.\n");
    printf(" -s : server for websocket. Defaults libws.\n");
    printf(" -p : network port to connect to. Defaults to 8080.\n");
    printf(" --help : display this message.\n");
    printf(" --quiet : don't print error messages.\n");
    printf("\nSee https://github.com/zhoukk/libws for more information.\n\n");
    exit(0);
}

static void
config(int argc, char *argv[]) {
    int i;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-p") || !strcmp(argv[i], "--port")) {
            if (i == argc-1) {
                fprintf(stderr, "Error: -p argument given but no port specified.\n\n");
                goto e;
            } else {
                port = atoi(argv[i+1]);
                if (port < 1 || port > 65535) {
                    fprintf(stderr, "Error: Invalid port given: %d\n", port);
                    goto e;
                }
            }
            i++;
        } else if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--debug")) {
            debug = 1;
        } else if (!strcmp(argv[i], "--help")) {
            usage();
        } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--host")) {
            if (i == argc-1) {
                fprintf(stderr, "Error: -h argument given but no host specified.\n\n");
                goto e;
            } else {
                host = strdup(argv[i+1]);
            }
            i++;
        } else if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "--server")) {
            if (i == argc-1) {
                fprintf(stderr, "Error: -u argument given but no server specified.\n\n");
                goto e;
            } else {
                server = strdup(argv[i+1]);
            }
            i++;
        } else if (!strcmp(argv[i], "--quiet")) {
            quiet = 1;
        } else {
            fprintf(stderr, "Error: Unknown option '%s'.\n", argv[i]);
            goto e;
        }
    }
    return;

e:
    fprintf(stderr, "\nUse 'libws_server --help' to see usage.\n");
    exit(0);
}


static int
__on_message_begin(http_parser *p) {
    (void)p;
    if (!quiet) fprintf(stdout, "__on_message_begin\n");
    return 0;
}

static int
__on_url(http_parser *p, const char *at, size_t length) {
    (void)p;
    if (!quiet) fprintf(stdout, "__on_url %.*s\n", (int)length, at);
    return 0;
}

static int
__on_status(http_parser *p, const char *at, size_t length) {
    (void)p;
    if (!quiet) fprintf(stdout, "__on_status %.*s\n", (int)length, at);
    return 0;
}

static int
__on_header_field(http_parser *p, const char *at, size_t length) {
    struct ae_io *io = (struct ae_io *)p->data;
    io->header_at = at;
    io->header_length = length;
    if (!quiet) fprintf(stdout, "__on_header_field %.*s\n", (int)length, at);
    return 0;
}

static int
__on_header_value(http_parser *p, const char *at, size_t length) {
    struct ae_io *io = (struct ae_io *)p->data;
    int flag = libws__valid_header(&io->flags, io->header_at, io->header_length, at, length);
    if (flag == WS_HEADER_KEY) {
        strncpy(io->key, at, length);
    }
    if (flag == WS_HEADER_PROTOCOL) {
        io->protocol = strndup(at, length);
    }
    if (!quiet) fprintf(stdout, "__on_header_value %.*s\n", (int)length, at);
    return 0;
}

static int
__on_headers_complete(http_parser *p) {
    struct ae_io *io = (struct ae_io *)p->data;
    if (!quiet) fprintf(stdout, "__on_headers_complete\n");
    if (io->flags != WS_HEADER_REQ) {
        return -1;
    }
    io->handshake = 1;
    return 0;
}

static int
__on_body(http_parser *p, const char *at, size_t length) {
    (void)p;
    if (!quiet) fprintf(stdout, "__on_body %.*s\n", (int)length, at);
    return 0;
}

static int
__on_message_complete(http_parser *p) {
    struct ae_io *io = (struct ae_io *)p->data;
    if (!quiet) fprintf(stdout, "__on_message_complete\n");
    if (io->handshake) {
        char response[4096];
        int n = libws__response(response, 4096, server, io->protocol, io->key, io->accept);
        anetWrite(io->fd, response, n);
    }
    return 0;
}

static void
__close(aeEventLoop *el, struct ae_io *io) {
    if (AE_ERR != io->fd) {
        aeDeleteFileEvent(el, io->fd, AE_READABLE);
        close(io->fd);
    }
    free(io->protocol);
    free(io);
}

static void
__read(aeEventLoop *el, int fd, void *privdata, int mask) {
    struct ae_io *io;
    int nread;
    char buff[4096];
    (void)mask;
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

    io = (struct ae_io *)privdata;
    nread = read(fd, buff, sizeof(buff));
    if (nread == -1 && errno == EAGAIN) {
        return;
    }
    if (nread <= 0) {
        __close(el, io);
        return;
    }
    if (!io->handshake) {
        int parsed = http_parser_execute(&io->http_p, &settings, buff, nread);
        if (io->http_p.http_errno) {
            if (!quiet) fprintf(stderr, "http_parser_execute: %s %s\n", http_errno_name(io->http_p.http_errno), http_errno_description(io->http_p.http_errno));
            __close(el, io);
            return;
        }
        if (parsed != nread) {
            if (!quiet) fprintf(stdout, "http_parser_execute parsed: %d, nread:%d\n", parsed, nread);
        }
    } else {
        struct libws_b b = {.data = buff, .length = nread};
        struct libws_frame f;
        int rc;

        while ((rc = libws__parser_execute(&io->ws_p, &b, &f)) > 0) {
            fprintf(stdout, "opcode:%d, payload:%.*s\n", f.opcode, (int)f.payload.length, f.payload.data);
            uint64_t size = libws__build_size(0, f.payload.length);
            char *data = malloc(size);
            int flags = 0;
            WS_BUILD_OPCODE(flags, f.opcode);
            WS_BUILD_FIN(flags);
            libws__build(data, flags, &f.payload);
            anetWrite(io->fd, data, size);
            free(data);
            if (f.payload.data) free(f.payload.data);
        }
        if (rc) {
            __close(el, io);
            return;
        }
    }
}

static void
__connection(aeEventLoop *el, int fd, char *ip) {
    int keepalive = 300;
    struct ae_io *io;
    (void)ip;

    io = (struct ae_io *)malloc(sizeof *io);
    memset(io, 0, sizeof *io);

    anetNonBlock(0, fd);
    anetEnableTcpNoDelay(0, fd);
    anetKeepAlive(0, fd, keepalive);

    if (aeCreateFileEvent(el, fd, AE_READABLE, __read, io) == AE_ERR) {
        if (!quiet) fprintf(stderr, "aeCreateFileEvent AE_READABLE __read fail\n");
        close(fd);
        free(io);
        return;
    }

    io->fd = fd;
    http_parser_init(&io->http_p, HTTP_REQUEST);
    io->http_p.data = io;
    libws__parser_init(&io->ws_p);
}

static void
__accept(aeEventLoop *el, int fd, void *privdata, int mask) {
    int cport, cfd, max = 100;
    char cip[46];
    (void)privdata;
    (void)el;
    (void)mask;

    while(max--) {
        char neterr[ANET_ERR_LEN];
        cfd = anetTcpAccept(neterr, fd, cip, sizeof cip, &cport);
        if (cfd == ANET_ERR) {
            if (errno != EWOULDBLOCK)
                if (!quiet) fprintf(stderr, "anetTcpAccept: %s\n", neterr);
            return;
        }
        if (!quiet) fprintf(stdout, "__accept %s:%d\n", cip, cport);
        __connection(el, cfd, cip);
    }
}

static int
__listen(aeEventLoop *el, char *host, int port) {
    char neterr[ANET_ERR_LEN];
    int fd = anetTcpServer(neterr, port, host, 512);
    if (fd == ANET_ERR) {
        fprintf(stderr, "anetTcpServer: %s\n", neterr);
        return -1;
    }
    anetNonBlock(0, fd);
    if (aeCreateFileEvent(el, fd, AE_READABLE, __accept, 0) == AE_ERR) {
        fprintf(stderr, "aeCreateFileEvent AE_READABLE __accept failed\n");
        return -1;
    }
    fprintf(stdout, "libws_server listen at %s:%d\n", host, port);
    return fd;
}

int
main(int argc, char *argv[]) {
    config(argc, argv);
    if (!host) {
        host = strdup("0.0.0.0");
    }
    if (!server) {
        server = strdup("libws");
    }

    aeEventLoop *el;
    int fd;
    el = aeCreateEventLoop(128);
    fd = __listen(el, host, port);
    if (fd == ANET_ERR) {
        return 0;
    }

    aeMain(el);
    aeDeleteEventLoop(el);
    close(fd);

    free(host);
    free(server);
    return 0;
}
