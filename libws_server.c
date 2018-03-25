#define LIBWSHTTP_IMPLEMENTATION
#include "libwshttp.h"

#include "lib/ae.h"
#include "lib/anet.h"

#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>

struct ae_io {
    int fd;
    struct libwshttp *wh;
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

static void
__close(aeEventLoop *el, struct ae_io *io) {
    if (AE_ERR != io->fd) {
        aeDeleteFileEvent(el, io->fd, AE_READABLE);
        close(io->fd);
    }
    libwshttp__destroy(io->wh);
    free(io);
}

static void
__read(aeEventLoop *el, int fd, void *privdata, int mask) {
    struct ae_io *io;
    int nread;
    char buff[4096];
    struct libws_b b;
    struct libwshttp_event evt;
    int rc;
    (void)mask;

    io = (struct ae_io *)privdata;
    nread = read(fd, buff, sizeof(buff));
    if (nread == -1 && errno == EAGAIN) {
        return;
    }
    if (nread <= 0) {
        __close(el, io);
        return;
    }
    
    b.data = buff;
    b.length = nread;

    while ((rc = libwshttp__feed(io->wh, &b, &evt)) > 0) {
        if (evt.event == LIBWSHTTP_OPEN) {

        } else if (evt.event == LIBWSHTTP_DATA) {
            fprintf(stdout, "opcode:%d, payload:%.*s\n", evt.f.opcode, (int)evt.f.payload.length, evt.f.payload.data);
            libwshttp__write(io->wh, WS_OPCODE_BINARY, &evt.f.payload);
            free(evt.f.payload.data);
        }
    }
    if (rc) {
        shutdown(fd, SHUT_WR);
    }
}

static int
_write(void *inst, const char *data, int size) {
    struct ae_io *io;

    io = (struct ae_io *)inst;
    return anetWrite(io->fd, (char *)data, size);
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
    io->wh = libwshttp__create(1, io, _write);
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
