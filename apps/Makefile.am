plugindir=$(pkglibdir)/plugins
AM_CPPFLAGS = -I$(top_srcdir)/include

AM_CFLAGS = \
@GMERLIN_CFLAGS@ \
-DLOCALE_DIR=\"$(localedir)\" \
-DPLUGIN_DIR=\"$(plugindir)\" \
-DDOC_DIR=\"$(docdir)\" \
-DDATA_DIR=\"$(pkgdatadir)\"

AM_LDFLAGS = @GMERLIN_EXE_LDFLAGS@

bin_PROGRAMS = gavf-cat gavf-info

gavf_cat_SOURCES = gavf-cat.c
gavf_info_SOURCES = gavf-info.c

gavf_cat_LDADD = ../lib/libgavf.la @GMERLIN_LIBS@ @GAVL_LIBS@
gavf_info_LDADD = ../lib/libgavf.la @GMERLIN_LIBS@ @GAVL_LIBS@

