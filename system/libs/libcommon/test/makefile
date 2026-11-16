#Makefile for Test1
COPTS = -Wall -c -O2 -fexceptions -D__DEBUG__

APPBIN = /Applications/Test1
OBJS = main.o application.o mainwindow.o

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

all : translations objs "Test1"


translations:

objs:
	@mkdir -p objs

"Test1": $(OBJS)
	@echo Linking...
	@$(CXX) $(OBJS) -o "Test1"  -lsyllable -lcommon
	@echo Adding resources...
	@rescopy "Test1" -r icon48x48.png 
	@addattrib "Test1" os::Category Other

clean:
	@echo Cleaning...
	@rm -f $(OBJDIR)/*
	@rm -f "Test1"

deps:

install: all
	@echo Installing...
	@mkdir -p $(IMAGE)/Applications/Test1
	@cp "Test1" "$(IMAGE)/Applications/Test1/Test1"

dist: install

