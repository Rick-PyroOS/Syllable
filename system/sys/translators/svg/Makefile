OBJS = svgtrans.o
OBJDIR = objs

TRANSDIR = /system/extensions/translators

#include $(ATHEOS_SRC)/makefile.rules

all :	$(OBJDIR) $(OBJDIR)/svgtrans.so

$(OBJDIR):
	mkdir $(OBJDIR)

$(OBJDIR)/svgtrans.so: $(OBJS)
	$(CXX) $(CXXFLAGS) -shared $(OBJS) -lsyllable -o $(OBJDIR)/svgtrans.so

clean:
	rm -f *.o $(OBJDIR)/*.d $(OBJDIR)/*.so

install:
	install $(OBJDIR)/svgtrans.so $(IMAGE)/$(TRANSDIR)/

dist: install

-include $(OBJDIR)/*.d 
