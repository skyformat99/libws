ACLOCAL_AMFLAGS = -I m4

include_HEADERS = libws.h

EXTRA_DIST =  lib/ae.h lib/anet.h lib/fmacros.h lib/zmalloc.h lib/config.h lib/ae_epoll.c lib/ae_evport.c lib/ae_kqueue.c lib/ae_select.c

bin_PROGRAMS = libws_client libws_server

libws_client_SOURCES = libws_client.c http_parser.c lib/ae.c lib/anet.c lib/zmalloc.c
libws_client_CFLAGS = -Wall -Werror -Wextra -I/usr/local/Cellar/openssl/1.0.2j/include
libws_client_LDFLAGS = -L/usr/local/Cellar/openssl/1.0.2j/lib
libws_client_LDADD = -lssl -lcrypto

libws_server_SOURCES = libws_server.c http_parser.c lib/ae.c lib/anet.c lib/zmalloc.c
libws_server_CFLAGS = -Wall -Werror -Wextra -I/usr/local/Cellar/openssl/1.0.2j/include
libws_server_LDFLAGS = -L/usr/local/Cellar/openssl/1.0.2j/lib
libws_server_LDADD = -lssl -lcrypto

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libws.pc
