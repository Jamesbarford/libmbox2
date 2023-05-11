libmbox2_sources = mbox-buf.c \
				   mbox-date.c \
				   mbox-list.c \
				   mbox-msg.c \
				   mbox-redblacktree.c \
				   mbox-timing.c \
				   mbox-io.c \
				   mbox-worker.c \
				   mbox-parser.c \
				   mbox-common-headers.c \
				   mbox-index.c \
				   mbox-memory.c \
				   mbox-array.c \
				   mbox.c

libmbox2_headers = mbox-buf.h \
				   mbox-date.h \
				   mbox-list.h \
				   mbox-msg.h \
				   mbox-redblacktree.h \
				   mbox-timing.h \
				   mbox-mbox-io.h \
				   mbox-worker.h \
				   mbox-parser.h \
				   mbox-common-headers.h \
				   mbox-index.h \
				   mbox-memory.h \
				   mbox-array.h \
				   mbox.h

# Library name and version
lib_LTLIBRARIES = libmbox2.la
libmbox2_la_LDFLAGS = -version-info 1:0:0
# Include directories
AM_CPPFLAGS = -I$(top_srcdir)

# Library compilation options
AM_CFLAGS = -Wall -lpthread

# Set up the library build
libmbox2_la_SOURCES = $(libmbox2_sources) $(libmbox2_headers)
