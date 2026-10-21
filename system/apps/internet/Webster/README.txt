Dependencies:

 libpng
 libjpeg
 ICU 3.6
 LibXML2 2.6.29
 OpenSSL 0.9.8e
 SQLite 3.5.5

 c-ares 1.3.2 (included in Syllable)
 cURL 7.16.0 (included in Syllable)

Tools:

 GCC 3.4.3
 Perl 5.8.8
 FLex 2.5.33 release 3
 GPerf 3.0.2

Symlinks:

 /usr/bin/gcc -> /usr/indexes/bin/gcc
 /usr/bin/perl -> /usr/indexes/bin/perl
 /usr/local/bin/perl -> /usr/indexes/bin/perl
 /usr/local/bin/ranlib -> /usr/indexes/bin/ranlib

Building:

You must use GCC 3.4.3. Using any other version of GCC will result in a
version of Webster which will not run!

Ensure you have release 3 or later of FLex installed. If FLex exits during
the build with an 'Internal error', you should download and re-install the
latest version available from the Syllable website.

To build Webster, simply run "make" at the top level directory. This will
build JavaScriptCore, WebCore, WebView and Webster. Once the build is complete
you can run "make install" to install Webster and it's libraries to
/Applications/Webster. You can also run "make clean" to remove all of the
binary and object files.
