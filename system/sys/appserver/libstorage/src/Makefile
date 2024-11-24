CFLAGS   += -c -fexceptions
CXXFLAGS += -c -fexceptions -I.
AOPTS    = -c


LIBBIN = /system/libraries

LIBNAME = libstorage.so.1

VPATH = ./

OBJS	= attribute.o directory.o directory_create.cpp file.o filereference.o \
		  filereference_info.o filereference_rename.o fsnode.o memfile.o nodemonitor.o \
		  operation_copy.o operation_delete.o operation_move.o path.o registrar.o \
		  seekableio.o streamable.o symlink.o tempfile.o volumes.o

OBJDIR	:= objs

include $(ATHEOS_SRC)/makefile.rules

all :	$(OBJDIR) $(OBJDIR)/$(LIBNAME)

$(OBJDIR):
	mkdir $(OBJDIR)
	mkdir $(LIB_PATH)

$(OBJDIR)/$(LIBNAME): $(OBJS)
	$(CXX) -shared -Xlinker "-soname=$(LIBNAME)" $(OBJS) -lm -lutil -o $(LIB_PATH)/$(LIBNAME)
	ln -sf $(LIB_PATH)/$(LIBNAME) $(LIB_PATH)/libstorage.so


install: $(OBJDIR)/$(LIBNAME)
	install -s $(OBJDIR)/$(LIBNAME) $(IMAGE)/$(LIBBIN)/$(LIBNAME)
	ln -sf $(LIBNAME) $(IMAGE)/$(LIBBIN)/libstorage.so

dist: install


clean:
	-rm -f $(OBJS) $(OBJDIR)/*


