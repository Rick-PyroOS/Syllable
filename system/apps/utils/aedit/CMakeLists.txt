# AEdit: text editor (applications/AEdit/AEdit, linked as system/programs/aedit)
syllable_program(aedit OUTPUT_NAME AEdit
  SOURCES main.cpp appwindow.cpp editview.cpp aboutdialog.cpp gotodialog.cpp finddialog.cpp
          replacedialog.cpp buffer.cpp mytabview.cpp tabview.cpp icons.cpp dialog.cpp
  OPTIONS -Wno-deprecated LIBS libsyllable STRIP INSTALL applications/AEdit)
syllable_resources(aedit ADD
  stock_copy_16=pixmaps/stock_copy_16.png
  stock_copy_24=pixmaps/stock_copy_24.png
  stock_cut_16=pixmaps/stock_cut_16.png
  stock_cut_24=pixmaps/stock_cut_24.png
  stock_exit_16=pixmaps/stock_exit_16.png
  stock_jump_to_16=pixmaps/stock_jump_to_16.png
  stock_left_arrow_16=pixmaps/stock_left_arrow_16.png
  stock_new_16=pixmaps/stock_new_16.png
  stock_new_24=pixmaps/stock_new_24.png
  stock_open_16=pixmaps/stock_open_16.png
  stock_open_24=pixmaps/stock_open_24.png
  stock_paste_16=pixmaps/stock_paste_16.png
  stock_paste_24=pixmaps/stock_paste_24.png
  stock_redo_16=pixmaps/stock_redo_16.png
  stock_redo_24=pixmaps/stock_redo_24.png
  stock_right_arrow_16=pixmaps/stock_right_arrow_16.png
  stock_save_16=pixmaps/stock_save_16.png
  stock_save_24=pixmaps/stock_save_16.png
  stock_save_as_16=pixmaps/stock_save_as_16.png
  stock_search_16=pixmaps/stock_search_16.png
  stock_search_24=pixmaps/stock_search_24.png
  stock_search_replace_16=pixmaps/stock_search_replace_16.png
  stock_undo_16=pixmaps/stock_undo_16.png
  stock_undo_24=pixmaps/stock_undo_24.png
  stock_select_all_16=pixmaps/stock_select_all_16.png
  stock_close_16=pixmaps/stock_close_16.png
  stock_font_16=pixmaps/stock_font_16.png
  icon24x24.png=pixmaps/icon24x24.png
  icon48x48.png=pixmaps/icon48x48.png
  text_plain.png=pixmaps/text_plain.png
  about=pixmaps/about.png)
syllable_catalogs(aedit NAME aedit DIR resources LANGS bg da de el es fr fy hu it nl pl pt-br ro ru sv EMBED)
install(CODE "file(CREATE_LINK /applications/AEdit/AEdit
  \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/system/programs/aedit SYMBOLIC)")
