CFLAGS   += -c -fexceptions -fno-PIC
CXXFLAGS += -c -fexceptions -fno-PIC
AOPTS    = -c
#LIBS     = -los
LIBS     = -los

CFLAGS += -D__ENABLE_DEBUG__

C_INCLUDE_PATH=$(PYRO_INCLUDE_PATH)

DEVBIN = /system/drivers/servers/inputserver/filters
OBJS =	voyeur.o voyeurwindow.o


OBJDIR	:= objs
OBJS	:= $(addprefix $(OBJDIR)/,$(OBJS))


#
#	rules
#

$(OBJDIR)/%.o : %.c
	$(CC) $(CFLAGS) $< -o $@

$(OBJDIR)/%.o : %.cpp
	$(CXX) $(CFLAGS) $< -o $@


all :	objs $(OBJDIR)/voyeur

deps:

objs:
	mkdir objs

$(OBJDIR)/voyeur: $(OBJS)
	$(CXX) -plugin -Xlinker -Bsymbolic $(OBJS) -nodefaultlibs -o $(OBJDIR)/voyeur

install: $(OBJDIR)/voyeur
	cp -f $(OBJDIR)/voyeur $(IMAGE)/$(DEVBIN)/voyeur

dist: install

clean:
	-rm $(OBJS) $(OBJDIR)/voyeur
