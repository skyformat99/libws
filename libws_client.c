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
    if (flag == WS_HEADER_ACCEPT) {
        strncpy(io->accept, at, length);
    }
    if (!quiet) fprintf(stdout, "__on_header_value %.*s\n", (int)length, at);
    return 0;
}

static int
__on_headers_complete(http_parser *p) {
    struct ae_io *io = (struct ae_io *)p->data;
    if (!quiet) fprintf(stdout, "__on_headers_complete\n");
    if (p->status_code != HTTP_STATUS_SWITCHING_PROTOCOLS) {
        return -1;
    }
    if (io->flags != WS_HEADER_RSP) {
        return -1;
    }
    if (libws__handshake(io->key, io->accept)) {
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
        struct libws_b b = {.data = payload, .length = length};
        uint64_t size = libws__build_size(1, length);
        char *data = malloc(size);
        int flags = 0;
        WS_BUILD_OPCODE(flags, WS_OPCODE_TEXT);
        WS_BUILD_FIN(flags);
        WS_BUILD_MASK(flags);
        libws__build(data, flags, &b);
        anetWrite(io->fd, data, size);
        free(data);
    }
    return 0;
}

static void
__close(aeEventLoop *el, struct ae_io *io) {
    if (AE_ERR != io->fd) {
        aeDeleteFileEvent(el, io->fd, AE_READABLE);
        close(io->fd);
    }
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
        aeStop(el);
        return;
    }
    if (!io->handshake) {
        int parsed = http_parser_execute(&io->http_p, &settings, buff, nread);
        if (io->http_p.http_errno) {
            if (!quiet) fprintf(stderr, "http_parser_execute: %s %s\n", http_errno_name(io->http_p.http_errno), http_errno_description(io->http_p.http_errno));
            __close(el, io);
            aeStop(el);
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
            if (f.payload.data) free(f.payload.data);
        }
        if (rc) {
            __close(el, io);
            aeStop(el);
            return;
        }
    }
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
    http_parser_init(&io->http_p, HTTP_RESPONSE);
    io->http_p.data = io;
    libws__parser_init(&io->ws_p);

    char request[4096];
    int n = libws__request(request, 4096, url, host, host, protocol, io->key);
    anetWrite(io->fd, request, n);

    aeMain(el);
    aeDeleteEventLoop(el);

    free(host);
    free(url);
    free(protocol);
    if (payload)
        free(payload);
    return 0;
}
