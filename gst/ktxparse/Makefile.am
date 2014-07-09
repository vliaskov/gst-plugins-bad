# Note: plugindir is set in configure

##############################################################################
# TODO: change libgstktxparse.la to something else, e.g. libmysomething.la     #
##############################################################################
plugin_LTLIBRARIES = libgstktxparse.la

##############################################################################
# TODO: for the next set of variables, name the prefix if you named the .la, #
#  e.g. libmysomething.la => libmysomething_la_SOURCES                       #
#                            libmysomething_la_CFLAGS                        #
#                            libmysomething_la_LIBADD                        #
#                            libmysomething_la_LDFLAGS                       #
##############################################################################

# sources used to compile this plug-in
libgstktxparse_la_SOURCES = gstktxparse.c gstktxparse.h

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstktxparse_la_CFLAGS = $(GST_CFLAGS)
libgstktxparse_la_LIBADD = $(GST_LIBS)
libgstktxparse_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstktxparse_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstktxparse.h
