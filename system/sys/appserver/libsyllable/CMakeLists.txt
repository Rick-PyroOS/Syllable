# libsyllable: Syllable's application framework (GUI, storage, messaging...)

set(SOURCES
  gui/rect.cpp gui/region.cpp gui/desktop.cpp gui/bitmap.cpp gui/view.cpp gui/tabview.cpp
  gui/window.cpp gui/slider.cpp gui/scrollbar.cpp gui/spinner.cpp gui/frameview.cpp
  gui/checkbox.cpp gui/textview.cpp gui/button.cpp gui/font.cpp gui/stdbitmaps.cpp
  gui/control.cpp gui/stringview.cpp gui/menu.cpp gui/tableview.cpp gui/tableview_v2.cpp
  gui/sprite.cpp gui/progressbar.cpp gui/listview.cpp gui/checkrow.cpp gui/requesters.cpp
  gui/layoutview.cpp gui/radiobutton.cpp gui/filerequester.cpp gui/dropdownmenu.cpp
  gui/checkmenu.cpp gui/splitter.cpp gui/image.cpp gui/imageview.cpp gui/imagebutton.cpp
  gui/treeview.cpp gui/checknode.cpp gui/popupmenu.cpp gui/iconview.cpp gui/icondirview.cpp
  gui/statusbar.cpp gui/toolbar.cpp gui/separator.cpp gui/inputbox.cpp gui/tooltip.cpp
  gui/calendarview.cpp gui/dateview.cpp gui/radiomenu.cpp gui/splash.cpp
  gui/colorselector/colorselector.cpp gui/colorselector/colorview.cpp
  gui/colorselector/colorwheel.cpp gui/colorselector/gradientview.cpp
  gui/fontrequester/fontrequester.cpp gui/fontrequester/fontrequesterview.cpp
  gui/colorrequester.cpp
  util/appserverconfig.cpp util/looper.cpp util/handler.cpp util/application.cpp
  util/message.cpp util/messenger.cpp util/messagequeue.cpp util/invoker.cpp
  util/clipboard.cpp util/string.cpp util/regexp.cpp util/variant.cpp
  util/circularbuffer.cpp util/resources.cpp util/optionparser.cpp util/settings.cpp
  util/thread.cpp util/messagefilter.cpp util/event.cpp
  storage/filereference.cpp storage/filereference_info.cpp storage/filereference_rename.cpp
  storage/operation_copy.cpp storage/operation_move.cpp storage/operation_delete.cpp
  storage/fsnode.cpp storage/streamableio.cpp storage/seekableio.cpp storage/file.cpp
  storage/tempfile.cpp storage/directory.cpp storage/directory_create.cpp storage/symlink.cpp
  storage/nodemonitor.cpp storage/path.cpp storage/memfile.cpp storage/registrar.cpp
  storage/volumes.cpp util/datetime.cpp util/shortcutkey.cpp util/catalog.cpp util/keymap.cpp
  util/locale.cpp storage/attribute.cpp translation/translator.cpp util/random.cpp
  util/vstubs.s)
# util/attributesettings.cpp (in the Makefile since 3f72a80d) includes
# <os/kernel.h>, <os/types.h>, <os/fs_attribs.h> and <os/string.h>, which do
# not exist in the tree yet, and nothing uses AttributeSettings; off until
# they do.
option(SYLLABLE_ATTRIBUTE_SETTINGS "Build os::AttributeSettings (needs the os/ headers)" OFF)
if(SYLLABLE_ATTRIBUTE_SETTINGS)
  list(APPEND SOURCES util/attributesettings.cpp)
endif()

# preprocessed assembler, as makefile.rules builds .s files
set_source_files_properties(util/vstubs.s PROPERTIES LANGUAGE C COMPILE_OPTIONS "-x;assembler-with-cpp")

syllable_library(libsyllable SONAME libsyllable.so.8
  SOURCES ${SOURCES}
  OPTIONS -fexceptions
  LIBS m
  LINKS libos.so
  INSTALL system/libraries STRIP)
syllable_catalogs(libsyllable NAME libsyllable DIR catalogs
  LANGS da de es fr hu nl pl pt-br ru sv
  INSTALL system/resources/catalogs)
