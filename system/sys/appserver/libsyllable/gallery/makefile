#Makefile for WidgetGallery
COPTS = -Wall -c -O2 -fexceptions

OBJS = main.o application.o common.o mainwindow.o views/view.o views/button/view.o views/controls/view.o views/dates/view.o views/label/view.o views/lists/view.o views/menus/view.o views/textview/view.o views/views/view.o
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

all : translations objs "Gallery-libsyllable"

translations:

objs:
	@mkdir -p objs
	@mkdir -p objs/views
	@find views/ -type d -exec mkdir -p objs/{} \;

"Gallery-libsyllable": $(OBJS)
	@echo Linking...
	@$(CXX) $(OBJS) -o "Gallery-libsyllable"  -los -lstdc++
	@echo Adding resources...
	@rescopy "Gallery-libsyllable" -r resources/icons/*
	@addattrib "Gallery-libsyllable" os::Category Demos

clean:
	@echo Cleaning...
	@rm -rf objs
	@rm -f "Gallery-libsyllable"

deps:

install: all
	@echo Installing...
	@mkdir -p "$(IMAGE)/Applications/Demos/Libraries/Syllable"
	@cp "Gallery-libsyllable" "$(IMAGE)/Applications/Demos/Libraries/Syllable/Gallery"

dist: install