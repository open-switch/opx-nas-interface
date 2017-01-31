ACLOCAL_AMFLAGS=-I m4
SUBDIRS = inc

bin_SCRIPTS = scripts/bin/*

pyutilsdir=$(libdir)/opx
pyutils_SCRIPTS = scripts/lib/python/*.py

cfgdir = $(sysconfdir)/opx
cfg_SCRIPTS = scripts/*.xml

lib_LTLIBRARIES=libopx_nas_interface.la libopx_nas_meta_packet.la libopx_nas_packet_io.la

AM_CPPFLAGS=-D_FILE_OFFSET_BITS=64 -I$(top_srcdir)/inc/opx -I$(includedir)/opx
AM_CXXFLAGS=-std=c++11
AM_LDFLAGS=-shared -version-info 1:1:0 -levent

libopx_nas_interface_la_SOURCES=src/swp_util_tap.c src/nas_int_main.cpp \
         src/nas_int_common_obj.cpp src/nas_int_list.c \
         src/nas_int_ev_handlers.cpp src/vlan/nas_int_bridge.cpp \
         src/vlan/nas_int_vlan.cpp src/vlan/nas_vlan_cps.cpp \
         src/lag/nas_int_lag.c src/lag/nas_int_lag_api.cpp \
         src/lag/nas_int_lag_cps.cpp src/port/hal_int_utils.c \
         src/port/nas_int_logical_cps.cpp src/port/nas_int_port.cpp \
         src/port/nas_int_breakout.cpp src/port/nas_int_physical_cps.cpp \
         src/stats/nas_stats_if_cps.cpp src/stats/nas_stats_vlan_cps.cpp \
         src/vlan/nas_vlan_lag_api.cpp
libopx_nas_interface_la_LIBADD=-lopx_common -lopx_nas_common -lopx_nas_ndi -lopx_cps_api_common -lopx_logging -lopx_nas_linux -lopx_nas_platform -lpthread -levent

libopx_nas_meta_packet_la_SOURCES=src/nas_packet_meta.c
libopx_nas_meta_packet_la_LIBADD=-lopx_common -lopx_logging

libopx_nas_packet_io_la_SOURCES=src/packet_io.c
libopx_nas_packet_io_la_LIBADD=-lopx_common -lopx_logging libopx_nas_interface.la libopx_nas_meta_packet.la -lopx_nas_ndi -lopx_cps_api_common -lpthread

systemdconfdir=/lib/systemd/system
systemdconf_DATA = scripts/init/*.service
