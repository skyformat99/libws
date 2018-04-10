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

enum {
    MSGMODE_NONE,
    MSGMODE_FILE,
    MSGMODE_STDIN_LINE,
    MSGMODE_CMD,
    MSGMODE_NULL,
    MSGMODE_STDIN_FILE,
};

static char *host = 0;
static int port = 8080;
static int debug = 0;
static int quiet = 0;
static int pub_mode = MSGMODE_NONE;

static char *payload = 0;
static int length = 0;
static char *file_input = 0;
static char *url = 0;
static char *protocol = 0;


static void
usage(void) {
    printf("libws_client is a simple websocket client that will send a message to server and exit.\n");
    printf("libws_client version %s running on libws %d.%d.%d.\n\n", "0.0.0", 0, 2, 0);
    printf("Usage: libws_client [-h host] [-p port] [-u url] [-P protocol] {-f file | -l | -n | -m message}\n");
    printf("                     [-d] [--quiet]\n");
    printf("       libws_client --help\n\n");
    printf(" -d : enable debug messages.\n");
    printf(" -f : send the contents of a file as the message.\n");
    printf(" -h : http host to connect to. Defaults to localhost.\n");
    printf(" -u : url for websocket. Defaults /.\n");
    printf(" -P : protocol for websocket. Defaults ws.\n");
    printf(" -l : read messages from stdin, sending a separate message for each line.\n");
    printf(" -m : message payload to send.\n");
    printf(" -p : network port to connect to. Defaults to 8080.\n");
    printf(" -s : read message from stdin, sending the entire input as a message.\n");
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
        } else if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "--file")) {
            if (pub_mode != MSGMODE_NONE) {
                fprintf(stderr, "Error: Only one type of message can be sent at once.\n\n");
                goto e;
            } else if (i == argc-1) {
                fprintf(stderr, "Error: -f argument given but no file specified.\n\n");
                goto e;
            } else {
                pub_mode = MSGMODE_FILE;
                file_input = strdup(argv[i+1]);
            }
            i++;
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
        } else if (!strcmp(argv[i], "-u") || !strcmp(argv[i], "--url")) {
            if (i == argc-1) {
                fprintf(stderr, "Error: -u argument given but no url specified.\n\n");
                goto e;
            } else {
                url = strdup(argv[i+1]);
            }
            i++;
        } else if (!strcmp(argv[i], "-P") || !strcmp(argv[i], "--protocol")) {
            if (i == argc-1) {
                fprintf(stderr, "Error: -P argument given but no protocol specified.\n\n");
                goto e;
            } else {
                protocol = strdup(argv[i+1]);
            }
            i++;
        } else if (!strcmp(argv[i], "-l") || !strcmp(argv[i], "--stdin-line")) {
            if (pub_mode != MSGMODE_NONE) {
                fprintf(stderr, "Error: Only one type of message can be sent at once.\n\n");
                goto e;
            } else {
                pub_mode = MSGMODE_STDIN_LINE;
            }
        } else if (!strcmp(argv[i], "-m") || !strcmp(argv[i], "--message")) {
            if (pub_mode != MSGMODE_NONE) {
                fprintf(stderr, "Error: Only one type of message can be sent at once.\n\n");
                goto e;
            } else if (i == argc-1) {
                fprintf(stderr, "Error: -m argument given but no message specified.\n\n");
                goto e;
            } else {
                payload = strdup(argv[i+1]);
                length = strlen(payload);
                pub_mode = MSGMODE_CMD;
            }
            i++;
        } else if (!strcmp(argv[i], "--quiet")) {
            quiet = 1;
        } else if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "--stdin-file")) {
            if (pub_mode != MSGMODE_NONE) {
                fprintf(stderr, "Error: Only one type of message can be sent at once.\n\n");
                goto e;
            } else {
                pub_mode = MSGMODE_STDIN_FILE;
            }
        } else {
            fprintf(stderr, "Error: Unknown option '%s'.\n", argv[i]);
            goto e;
        }
    }
    return;

e:
    fprintf(stderr, "\nUse 'libws_client --help' to see usage.\n");
    exit(0);
}

static int
load_stdin_line(void) {
    char buff[1024];
    if (!fgets(buff, 1024, stdin))
        return -1;

    length = strlen(buff);
    if (buff[length-1] == '\n')
        buff[length-1] = '\0';
    length -= 1;
    payload = strdup(buff);
    return 0;
}

static int
load_stdin(void) {
    long pos = 0;
    char buf[1024];
    char *aux_message = 0;

    while (!feof(stdin)) {
        long rlen;
        rlen = fread(buf, 1, 1024, stdin);
        aux_message = realloc(payload, pos+rlen);
        if (!aux_message) {
            if (!quiet) fprintf(stderr, "Error: Out of memory.\n");
            free(payload);
            payload = 0;
            return 1;
        } else {
            payload = aux_message;
        }
        memcpy(&(payload[pos]), buf, rlen);
        pos += rlen;
    }
    length = pos;

    if (!length) {
        if (!quiet) fprintf(stderr, "Error: Zero length input.\n");
        return 1;
    }
    return 0;
}

static int
load_file(void) {
    long pos;
    FILE *fptr = 0;

    fptr = fopen(file_input, "rb");
    if (!fptr) {
        if (!quiet) fprintf(stderr, "Error: Unable to open file \"%s\".\n", file_input);
        return 1;
    }
    fseek(fptr, 0, SEEK_END);
    length = ftell(fptr);
    if (length > 268435455) {
        fclose(fptr);
        if (!quiet) fprintf(stderr, "Error: File \"%s\" is too large (>268,435,455 bytes).\n", file_input);
        return 1;
    } else if (length == 0) {
        fclose(fptr);
        if (!quiet) fprintf(stderr, "Error: File \"%s\" is empty.\n", file_input);
        return 1;
    } else if (length < 0) {
        fclose(fptr);
        if (!quiet) fprintf(stderr, "Error: Unable to determine size of file \"%s\".\n", file_input);
        return 1;
    }
    fseek(fptr, 0, SEEK_SET);
    payload = malloc(length);
    if (!payload) {
        fclose(fptr);
        if (!quiet) fprintf(stderr, "Error: Out of memory.\n");
        return 1;
    }
    pos = 0;
    while (pos < length) {
        long rlen;
        rlen = fread(&(payload[pos]), sizeof(char), length-pos, fptr);
        pos += rlen;
    }
    fclose(fptr);
    return 0;
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
        aeStop(el);
        return;
    }

    b.data = buff;
    b.length = nread;

    while ((rc = libwshttp__feed(io->wh, &b, &evt)) > 0) {
        if (evt.event == LIBWSHTTP_OPEN) {
            struct libws_b b = {.data = payload, .length = length};
            libwshttp__write(io->wh, WS_OPCODE_BINARY, &b);
        } else if (evt.event == LIBWSHTTP_DATA) {
            fprintf(stdout, "opcode:%d, payload:%.*s\n", evt.f.opcode, (int)evt.f.payload.length, evt.f.payload.data);
            free(evt.f.payload.data);
            libwshttp__close(io->wh, WS_STATUS_NORMAL, "byebye");
        } else if (evt.event == LIBWSHTTP_CLOSE) {
            if (evt.f.payload.length) {
                fprintf(stdout, "opcode:%d, status:%d, reason:%.*s\n", evt.f.opcode, WS_CLOSE_STATUS(evt.f.payload), WS_CLOSE_REASON_LEN(evt.f.payload), WS_CLOSE_REASON(evt.f.payload));
            } else {
                fprintf(stdout, "opcode:%d\n", evt.f.opcode);
            }
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
    return size == anetWrite(io->fd, (char *)data, size) ? 0 : -1;
}

static void
_close(void *inst) {
    struct ae_io *io;

    io = (struct ae_io *)inst;
    shutdown(io->fd, SHUT_WR);
}

static struct ae_io *
__connect(aeEventLoop *el, char *host, int port) {
    struct ae_io *io;
    int fd;
    char err[ANET_ERR_LEN];

    fd = anetTcpConnect(err, host, port);
    if (ANET_ERR == fd) {
        if (!quiet) fprintf(stderr, "anetTcpConnect: %s\n", err);
        goto e1;
    }
    anetNonBlock(0, fd);
    anetEnableTcpNoDelay(0, fd);
    anetTcpKeepAlive(0, fd);

    io = (struct ae_io *)malloc(sizeof *io);
    memset(io, 0, sizeof *io);

    if (AE_ERR == aeCreateFileEvent(el, fd, AE_READABLE, __read, io)) {
        if (!quiet) fprintf(stderr, "aeCreateFileEvent: error\n");
        goto e2;
    }

    io->fd = fd;
    io->wh = libwshttp__create(0, io, _write, _close);
    libwshttp__request(io->wh, url, host, protocol);
    return io;

e2:
    close(fd);
e1:
    return 0;
}

int
main(int argc, char *argv[]) {
    config(argc, argv);
    if (!host) {
        host = strdup("127.0.0.1");
    }
    if (!url) {
        url = strdup("/");
    }
    if (!protocol) {
        protocol = strdup("ws");
    }
    if (pub_mode == MSGMODE_STDIN_LINE) {
        if (load_stdin_line()) {
            fprintf(stderr, "Error loading input line from stdin.\n");
            return 0;
        }
    } else if (pub_mode == MSGMODE_STDIN_FILE) {
        if (load_stdin()) {
            fprintf(stderr, "Error loading input from stdin.\n");
            return 0;
        }
    } else if (pub_mode == MSGMODE_FILE && file_input) {
        if (load_file()) {
            fprintf(stderr, "Error loading input file \"%s\".\n", file_input);
            return 0;
        }
    }

    aeEventLoop *el;
    struct ae_io *io;
    el = aeCreateEventLoop(128);
    io = __connect(el, host, port);
    if (!io) {
        return 0;
    }

    aeMain(el);
    aeDeleteEventLoop(el);

    free(host);
    free(url);
    free(protocol);
    if (payload)
        free(payload);
    return 0;
}
