PLUGIN = npfilter${PLUGIN_SUFFIX}

SRCS = npfilter.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

LD = ${CXX}
CPPFLAGS += -I../..
CFLAGS += ${PLUGIN_CFLAGS}
