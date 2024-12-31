#Makefile for WidgetGallery
COPTS = -Wall -c -O2 -fexceptions

OBJS = main.o application.o common.o mainwindow.o views/view.o views/button/view.o views/controls/view.o views/label/view.o views/lists/view.o views/menus/view.o views/textview/view.o views/views/view.o
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

all : translations objs "WidgetGallery"

translations:

objs:
	@mkdir -p objs
	@mkdir -p objs/views
	@find views/ -type d -exec mkdir -p objs/{} \;

"WidgetGallery": $(OBJS)
	@echo Linking...
	@$(CXX) $(OBJS) -o "WidgetGallery"  -los -lstdc++
	@echo Adding resources...
	@rescopy "WidgetGallery" -r resources/icons/*
	@addattrib "WidgetGallery" os::Category Other

clean:
	@echo Cleaning...
	@rm -rf objs
	@rm -f "WidgetGallery"

deps:

install: all
	@echo Installing...
	@cp "WidgetGallery" "$(IMAGE)/Applications/WidgetGallery"

dist: install