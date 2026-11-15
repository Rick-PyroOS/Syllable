CFLAGS   = -c -fexceptions -ffast-math
CXXFLAGS = $(CFLAGS)
AOPTS   = -c
VPATH = ./
OBJDIR	:= objs
OBJS = Places.o popupmenu.o progressbar.o
APPBIN = /system/extensions/dock/
all :	$(OBJDIR) $(OBJS) $(OBJDIR)/Places.so

$(OBJDIR):
	mkdir $(OBJDIR)

$(OBJDIR)/Places.so: $(OBJS)
	$(CXX) -shared -Xlinker "-soname=Places.so" $(OBJS) -lsyllable -o $(OBJDIR)/Places.so
	rescopy -r $(OBJDIR)/Places.so ./images/*.png

install:  $(OBJDIR)/Places.so
	install -s $(OBJDIR)/Places.so $(APPBIN)/Places.so

dist:  $(OBJDIR)/Places.so
	install -s $(OBJDIR)/Places.so $(IMAGE)/$(APPBIN)/Places.so

deps: $(OBJDIR)

clean:
	rm -f $(OBJS) $(OBJDIR)/Places.so

-include $(OBJDIR)/*.d




