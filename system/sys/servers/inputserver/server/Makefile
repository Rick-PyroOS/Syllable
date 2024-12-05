CFLAGS   += -c -fexceptions
CXXFLAGS += -c -fexceptions
AOPTS    = -c
LIBS     = -los

CFLAGS += -D__ENABLE_DEBUG__
CXXFLAGS += -D__ENABLE_DEBUG__

C_INCLUDE_PATH=$(PYRO_INCLUDE_PATH)

SERVBIN = /system/servers

VPATH = ./


OBJS =  inputserver.o inputserverdevice.o inputserverfilter.o inputservermethod.o inputnode.o offlinewindow.o defaultkeymap.o keymap.o keyboard.o

OBJDIR	:= objs

include $(PYRO_SRC)/makefile.rules

all :	$(OBJDIR) $(OBJS) $(OBJDIR)/inputserver

$(OBJDIR):
	mkdir $(OBJDIR)

$(OBJDIR)/inputserver: $(OBJS)
	$(CXX) $(OBJS) $(LIBS) -o $(OBJDIR)/inputserver

install: $(OBJDIR)/inputserver
	cp -f $(OBJDIR)/inputserver $(IMAGE)/$(SERVBIN)/inputserver

dist: install

clean:
	-rm -f $(OBJS) $(OBJDIR)/inputserver
