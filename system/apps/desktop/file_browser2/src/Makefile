#Makefile for EFileBrowser
COPTS = -Wall -c -O2 -fexceptions

APPBIN = /Applications/EFileBrowser
OBJS = main.o application.o mainwindow.o common.o iconserver.o labels.o preferences.o shortcuts.o my_gui/address_field.o my_gui/address_field_button.o my_gui/sidebar_base.o my_gui/sidebar_title.o layouter.o statusbar.o syll_gui/icondirview.o syll_gui/iconview.o syll_gui/splitter.o syll_gui/tabview.o syll_gui/toolbar.o syll_gui/imagebutton.o syll_gui/radiomenu.o syll_gui/popupmenu.o tabs/tab_base.o tabs/tab_browser.o settings/keyboardsettings.o settings/settingsdialog.o settings/settingsnode.o

OBJDIR := objs
OBJS	:= $(addprefix $(OBJDIR)/,$(OBJS))

# Rules
$(OBJDIR)/%.o : %.c
	@echo Compiling : $<
	@$(CC) $(COPTS) $< -o $@

$(OBJDIR)/%.o : %.cpp
	@echo Compiling : $<
	@$(CXX) $(COPTS) $< -o $@

$(OBJDIR)/%.o : %.s
	@echo Assembling : $<
	@$(CC) $(COPTS) -x assembler-with-cpp $< -o $@

all : translations objs "EFileBrowser"


translations:
	@echo Creating catalogs...
	@catcomp -c "resources/lang.cd"

objs:
	@mkdir -p objs

"EFileBrowser": $(OBJS)
	@echo Linking...
	@$(CXX) $(OBJS) -o "EFileBrowser"  -lsyllable
	@echo Adding resources...
	@rescopy "EFileBrowser" -r images/*.png 
	@echo Adding catalogs...
	@rescopy "EFileBrowser" -a resources/lang.catalog
	@addattrib "EFileBrowser" os::Category Other

clean:
	@echo Cleaning...
	@rm -f $(OBJDIR)/*
	@rm -f "EFileBrowser"

deps:

install: all
	@echo Installing...
	@mkdir -p $(IMAGE)/Applications/EFileBrowser
	@cp "EFileBrowser" "$(IMAGE)/Applications/EFileBrowser/EFileBrowser"

dist: install

