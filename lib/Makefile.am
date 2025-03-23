AM_CPPFLAGS = -I$(top_srcdir)/include

AM_CFLAGS = -DLOCALE_DIR=\"$(localedir)\"

lib_LTLIBRARIES = libgavf.la

libgavf_la_SOURCES = \
gavf.c \
gavf_io.c \
gavftools.c \
packet_io.c

libgavf_la_LIBADD = \
  @GAVL_LIBS@ \
  @GMERLIN_LIBS@
