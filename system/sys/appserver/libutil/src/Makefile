CFLAGS   += -c -fexceptions
CXXFLAGS += -c -fexceptions -I.
AOPTS    = -c


LIBBIN = /system/libraries

LIBNAME = libutil.so.1

VPATH = ./

OBJS	= circularbuffer.o datetime.o \
		  optionparser.o regexp.o shortcutkey.o string.o

OBJDIR	:= objs

include $(ATHEOS_SRC)/makefile.rules

all :	$(OBJDIR) $(OBJDIR)/$(LIBNAME)

$(OBJDIR):
	mkdir $(OBJDIR)
	mkdir $(LIB_PATH)

$(OBJDIR)/$(LIBNAME): $(OBJS)
	$(CXX) -shared -Xlinker "-soname=$(LIBNAME)" $(OBJS) -lm -o $(LIB_PATH)/$(LIBNAME)
	ln -sf $(LIB_PATH)/$(LIBNAME) $(LIB_PATH)/libutil.so


install: $(OBJDIR)/$(LIBNAME)
	install -s $(OBJDIR)/$(LIBNAME) $(IMAGE)/$(LIBBIN)/$(LIBNAME)
	ln -sf $(LIBNAME) $(IMAGE)/$(LIBBIN)/libutil.so

dist: install


clean:
	-rm -f $(OBJS) $(OBJDIR)/*


