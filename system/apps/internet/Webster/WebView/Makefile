INCLUDES = -I../JavaScriptCore -I../JavaScriptCore/bindings -I../JavaScriptCore/wtf -I../WebCore -I../WebCore/dom -I../WebCore/platform -I../WebCore/page -I../WebCore/platform/graphics -I../WebCore/platform/graphics/svg -I../WebCore/editing -I../WebCore/xml \
			-I../WebCore/rendering -I../WebCore/loader -I../WebCore/loader/icon -I../WebCore/loader/syllable -I../WebCore/ss -I../WebCore/platform/network -I../WebCore/platform/syllable -I../WebCore/bindings/js -I../JavaScriptCore/kjs -I../WebCore/html \
			-I../WebCore/bridge -I../WebCore/DerivedSources/WebCore -I../WebCore/platform/text -I../WebCore/platform/sql -I../WebCore/platform/image-decoders -I../WebCore/platform/image-decoders/gif -I../WebCore/platform/image-decoders/bmp \
			-I../WebCore/platform/image-decoders/ico -I../WebCore/platform/image-decoders/jpeg -I../WebCore/platform/image-decoders/png \
			-I../WebCore/platform/image-decoders/xbm -I../WebCore/ForwardingHeaders -I../WebCore/css -I../WebCore/history \
			-I../WebCore/platform/network -I../WebCore/platform/network/syllable -Iinclude

CFLAGS   += -O2 -c -fexceptions $(INCLUDES) -DBUILDING_SYLLABLE__=1 -DUSE_SYSTEM_MALLOC=1 -D_CRTDBG_MAP_ALLOC -DHAVE_CONFIG_H=1
CXXFLAGS += -O2 -c -fexceptions $(INCLUDES) -DBUILDING_SYLLABLE__=1 -DUSE_SYSTEM_MALLOC=1 -D_CRTDBG_MAP_ALLOC -DHAVE_CONFIG_H=1

OBJS  = webview frameloaderclient chromeclient websettings downloaddialog
OBJS := $(addsuffix .o,$(OBJS))

OBJDIR = objs

VER  = 1
LIB  = libwebview.so
LIB := $(addsuffix .$(VER),$(LIB))

APPBIN = /Applications/Webster

include ../Makerules

all: $(OBJDIR) $(LIB)

$(OBJDIR):
	mkdir $(OBJDIR)

$(LIB): $(OBJS)
	$(CXX) -shared -Wl,-soname=$(LIB) -L. -L../JavaScriptCore/objs/ -ljavascriptcore -L../WebCore/objs/ -lwebcore -lsyllable $^ -o $@
	ln -sf $(LIB) libwebview.so

deps: $(OBJDIR) $(DEPS)

install:
	mkdir -p $(APPBIN)/lib
	cp -f $(LIB) $(APPBIN)/lib

clean:
	-rm -f $(OBJS) $(OBJDIR)/* libwebview.so*

-include $(OBJDIR)/%.d

