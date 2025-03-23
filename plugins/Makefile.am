## Process this file with automake to produce Makefile.in.

gmerlin_plugindir = @gmerlin_plugindir@

AM_CPPFLAGS = -I$(top_srcdir)/include

AM_CFLAGS = @GMERLIN_CFLAGS@ $(cdio_cflags) -DLOCALE_DIR=\"$(localedir)\"

AM_LDFLAGS = -avoid-version -module @GMERLIN_PLUGIN_LDFLAGS@

gmerlin_plugin_LTLIBRARIES = \
i_gavf.la

i_gavf_la_SOURCES = i_gavf.c
i_gavf_la_LIBADD = @GMERLIN_LIBS@ ../lib/libgavf.la

