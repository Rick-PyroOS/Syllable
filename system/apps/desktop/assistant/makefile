#Makefile for Assistant
COPTS = -Wall -c -O2 -fexceptions

APPBIN = /Applications/App
OBJS = main.o application.o mainwindow.o top_view.o

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

all : translations objs "Assistant"


translations:

objs:
	@mkdir -p objs
	@mkdir -p objs/gui
	@mkdir -p objs/providers/application
	
"Assistant": $(OBJS)
	@echo Linking...
	@$(CXX) $(OBJS) -o "Assistant"  -lsyllable
	@echo Adding resources...
	@rescopy "Assistant" -r *.png 
	@addattrib "Assistant" os::Category "System Tools"

clean:
	@echo Cleaning...
	@rm -f $(OBJDIR)/*
	@rm -f "Assistant"

deps:

install: all
	@echo Installing...
	@mkdir -p $(IMAGE)/Applications/App
	@cp "Assistant" "$(IMAGE)/Applications/App/Assistant"

dist: install

