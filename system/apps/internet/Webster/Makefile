all:
	$(MAKE) -C JavaScriptCore -f Makefile.syllable
	$(MAKE) -C WebCore -f Makefile.syllable
	$(MAKE) -C WebView
	$(MAKE) -C ABrowse_NG

clean:
	$(MAKE) -C JavaScriptCore -f Makefile.syllable clean
	$(MAKE) -C WebCore -f Makefile.syllable clean
	$(MAKE) -C WebView clean
	$(MAKE) -C ABrowse_NG clean

install:
	$(MAKE) -C ABrowse_NG install
	$(MAKE) -C WebView install
	$(MAKE) -C WebCore -f Makefile.syllable install
	$(MAKE) -C JavaScriptCore -f Makefile.syllable install
	cp -f /usr/icu/lib/libicudata.so.36.0 /Applications/Webster/lib
	ln -sf libicudata.so.36.0 /Applications/Webster/lib/libicudata.so.36
