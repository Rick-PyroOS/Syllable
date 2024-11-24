include $(ATHEOS_SRC)/makefile.rules
include $(ATHEOS_SRC)/recurse.rules

CFLAGS	+= -c
CXXFLAGS += -c
OBJDIR = objs

to_lower_OBJS		= to_lower.o
EXES = $(OBJDIR)/to_lower

all:  $(OBJDIR) $(EXES)



$(OBJDIR):
	-mkdir $(OBJDIR)

$(OBJDIR)/to_lower: $(to_lower_OBJS)
	$(CXX) $(to_lower_OBJS) -Wl,-rpath,$(LIB_PATH) -L$(LIB_PATH) -lutil -o $@


tests:
	DLL_PATH="${DLL_PATH}:${LIB_PATH}" $(OBJDIR)/to_lower