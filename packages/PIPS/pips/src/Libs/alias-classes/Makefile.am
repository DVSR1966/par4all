TARGET=alias-classes
BUILT_SOURCES=$(TARGET).h
include_HEADERS=$(TARGET).h

noinst_LTLIBRARIES=libalias-classes.la
libalias_classes_la_SOURCES=alias_pairs.c \
		alias_lists.c \
		alias_classes.c \
		points_to_set.c	\
                points_to_prettyprint.c \
		points_to_init_analysis.c \
		constant-path-utils.c \
	 	prettyprint.c \
		statement.c \
		expression.c \
		passes.c \
	        sinks.c \
		unstructured.c \
		variable.c \
		interprocedural.c \
		dereferencing.c

include $(srcdir)/../pipslibs_includes.mk
include $(top_srcdir)/makes/cproto.mk
