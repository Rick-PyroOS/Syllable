#Makefile for ctest
COPTS = -c -fexceptions -Wall -o2
LOPTS = -lsyllable -lrsvg

CATALOGS = sv

OBJS = main.o

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

all : objs first

objs:
	@echo Creating subdirectories...
	@mkdir -p objs
	
first: $(OBJS)
	@echo Linking...
	@$(CXX)   $(OBJS) -o svgviewer $(LOPTS)

clean:
	@echo Cleaning...
	@rm -r $(OBJDIR)
	@rm -f svgviewer


