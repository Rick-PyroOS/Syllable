CFLAGS   += -c -fexceptions
CXXFLAGS += -c -fexceptions
AOPTS    = -c
#LIBS     = -los
LIBS     = -lsyllable

CFLAGS += -D__ENABLE_DEBUG__
CXXFLAGS += -D__ENABLE_DEBUG__

C_INCLUDE_PATH=$(PYRO_INCLUDE_PATH)

WAITERBIN = /system/waiters

VPATH = ./


OBJS =  screensaver.o engine.o main.o

OBJDIR	:= objs

include $(PYRO_SRC)/makefile.rules

all :	$(OBJDIR) $(OBJS) $(OBJDIR)/screensaver

$(OBJDIR):
	mkdir $(OBJDIR)

$(OBJDIR)/screensaver: $(OBJS)
	$(CXX) $(OBJS) $(LIBS) -o $(OBJDIR)/screensaver

install: $(OBJDIR)/screensaver
	cp -f $(OBJDIR)/screensaver $(IMAGE)/$(WAITERBIN)/screensaver

dist: install

clean:
	-rm -f $(OBJS) $(OBJDIR)/screensaver
