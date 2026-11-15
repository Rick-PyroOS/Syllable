CFLAGS   += -c -fexceptions -ffast-math -I./include -I$(C_INCLUDE_PATH) -D__ENABLE_DEBUG__
CXXFLAGS += -c -fexceptions -ffast-math -I./include -I$(CPLUS_INCLUDE_PATH) -D__ENABLE_DEBUG__
AOPTS    = -c

LIBBIN = /system/libraries
LIBINC = /system/development/headers/pyro/common

LIBNAME = libcommon.so.1

VPATH = src

OBJS	= command.o

OBJDIR	:= objs

include $(PYRO_SRC)/makefile.rules

all :	$(OBJDIR) $(LIBNAME)

$(OBJDIR):
	mkdir $(OBJDIR)

$(LIBNAME): $(OBJS)
	$(CXX) -shared -Xlinker "-soname=$(LIBNAME)" $(OBJS) -lm -lsyllable -o $(LIBNAME)

deps:

install: $(LIBNAME)
	install -s $(LIBNAME) $(IMAGE)/$(LIBBIN)/$(LIBNAME)
	ln -sf $(LIBNAME) $(IMAGE)/$(LIBBIN)/libcommon.so
	mkdir -p $(LIBINC)
	cp -fv ./include/*.h $(LIBINC)

dist: install

doc:
#	-rm -R docs/*
#	doxygen libcommon.dox

install-doc:
#	-rm -R /os/Documentation/media/*
#	-mkdir -p /os/Documentation/media
#	cp -R docs/html/* /os/Documentation/media

deps: $(OBJDIR) $(DEPS)

clean:
	rm -f $(OBJS) $(OBJDIR)/*.d $(LIBNAME)

-include $(OBJDIR)/*.d

