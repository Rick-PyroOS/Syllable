INCLUDES = -I../JavaScriptCore -I../JavaScriptCore/bindings -I../JavaScriptCore/wtf -I../WebCore -I../WebCore/dom -I../WebCore/platform -I../WebCore/page -I../WebCore/platform/graphics -I../WebCore/platform/graphics/svg -I../WebCore/editing -I../WebCore/xml \
			-I../WebCore/rendering -I../WebCore/loader -I../WebCore/loader/icon -I../WebCore/loader/syllable -I../WebCore/ss -I../WebCore/platform/network -I../WebCore/platform/syllable -I../WebCore/bindings/js -I../JavaScriptCore/kjs -I../WebCore/html \
			-I../WebCore/bridge -I../WebCore/DerivedSources/WebCore -I../WebCore/platform/text -I../WebCore/platform/sql -I../WebCore/platform/image-decoders -I../WebCore/platform/image-decoders/gif -I../WebCore/platform/image-decoders/bmp \
			-I../WebCore/platform/image-decoders/ico -I../WebCore/platform/image-decoders/jpeg -I../WebCore/platform/image-decoders/png \
			-I../WebCore/platform/image-decoders/xbm -I../WebCore/ForwardingHeaders -I../WebCore/css -I../WebCore/history \
			-I../WebCore/platform/network -I../WebCore/platform/network/syllable

CFLAGS   += -O2 -c -fexceptions $(INCLUDES) -DBUILDING_SYLLABLE__=1 -DUSE_SYSTEM_MALLOC=1 -D_CRTDBG_MAP_ALLOC -DHAVE_CONFIG_H=1
CXXFLAGS += -O2 -c -fexceptions $(INCLUDES) -DBUILDING_SYLLABLE__=1 -DUSE_SYSTEM_MALLOC=1 -D_CRTDBG_MAP_ALLOC -DHAVE_CONFIG_H=1

APPBIN = /Applications/ABrowse
OBJS = main.o bookmarks.o HTMLView.o statusbar.o toolbar.o downloaddialog.o ChromeClientSyllable.o

OBJDIR = objs

include ../Makerules

all: $(OBJDIR) ABrowse

$(OBJDIR):
	mkdir $(OBJDIR)

ABrowse: version.h $(OBJS)
	g++ $(OBJS) -o ABrowse -L. -L../JavaScriptCore/objs/ -L../WebCore/objs/ -ljavascriptcore -lwebcore -lsyllable
	strip --strip-all ABrowse
	rescopy ABrowse -r ./bitmaps/*.png
	addattrib ABrowse os::Category Internet

version.h:
	printf "#define ABROWSE_VERSION\t\"%s-development\"\n" `date +%Y%m%d` > version.h

deps: $(OBJDIR) $(DEPS)

install: ABrowse
	mkdir -p $(APPBIN)
	cp -f ABrowse $(APPBIN)/ABrowse

clean:
	-rm -f $(OBJS) $(OBJDIR)/* ABrowse

-include $(OBJDIR)/*.d

