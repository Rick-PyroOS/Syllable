#Makefile for ServiceManager
COPTS = -Wall -c -O2 -fexceptions

APPBIN = /Applications/Noros
OBJS = main.o application.o mainwindow.o Executer/commonfuncs.o Executer/execute.o

OBJDIR := objs
OBJS	:= $(addprefix $(OBJDIR)/,$(OBJS))

# Rules
$(OBJDIR)/%.o : %.c
	@echo Compiling : $<
	@$(CC) $(COPTS) $< -o $@

$(OBJDIR)/%.o : %.cpp
	@echo Compiling : $<
	@$(CC) $(COPTS) $< -o $@

$(OBJDIR)/%.o : %.s
	@echo Assembling : $<
	@$(CC) $(COPTS) -x assembler-with-cpp $< -o $@

all : translations objs "ServiceManager"

$(OBJDIR)/mainwindow.o: mainwindowLayout.cpp

translations:

objs:
	@mkdir -p objs

"ServiceManager": $(OBJS)
	@echo Linking...
	@gcc $(OBJS) -o "ServiceManager"  -lstdc++ -lsyllable
	@echo Adding resources...
	@rescopy "ServiceManager" -r icon48x48.png 
	@addattrib "ServiceManager" os::Category "System Tools"

clean:
	@echo Cleaning...
	@rm -f $(OBJDIR)/*
	@rm -f "ServiceManager"

deps:

dist: all
	@echo Distribution...
	@mkdir -p $(DIST_DIR)/Applications/Noros
	@cp "ServiceManager" "$(DIST_DIR)/Applications/Noros/ServiceManager"

install: all
	@echo Installing...
	@mkdir -p /Applications/Noros
	@cp "ServiceManager" "/Applications/Noros/ServiceManager"

