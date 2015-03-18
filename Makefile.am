bin_PROGRAMS = wind

dist_man_MANS = wind.1

dist_doc_DATA = \
	LICENSE \
	NEWS \
	README

wind_SOURCES = \
	button.c \
	client.c \
	dragger.c \
	ewmh.c \
	frame.c \
	lib.c \
	list.h \
	main.c \
	mwm.c \
	root.c \
	wind.h \
	deleven.xbm \
	delodd.xbm

if CONFIG_XFT
wind_SOURCES+=xftfont.c
else
wind_SOURCES+=x11font.c
endif

wind_LDFLAGS = $(X_LIBS)
wind_LDADD = $(xft_LIBS) $(X_PRE_LIBS) -lX11 $(X_EXTRA_LIBS)
