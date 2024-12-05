CFLAGS   += -c -fexceptions -ffast-math -I./Include -O2 -Wall
CXXFLAGS += $(CFLAGS)
AFLAGS   = -c

CFLAGS += -D__ENABLE_DEBUG__ -D__PYRO__

C_INCLUDE_PATH=$(PYRO_INCLUDE_PATH)

LIBBIN = /system/libraries

LIBNAME = libopenal.so.1.6.372

VPATH = ./ Alc OpenAL32

OPENAL32_OBJS := alAuxEffectSlot.o alBuffer.o alEffect.o alError.o alExtension.o alFilter.o alListener.o \
			alSource.o alState.o alThunk.o OpenAL32.o

ALC_OBJS := ALc.o alcConfig.o alcRing.o alcThread.o ALu.o bs2b.o oss.o

OBJDIR			:= objs
OPENAL32_OBJS	:= $(addprefix $(OBJDIR)/,$(OPENAL32_OBJS))
ALC_OBJS		:= $(addprefix $(OBJDIR)/,$(ALC_OBJS))

include $(PYRO_SRC)/makefile.rules

all :	$(OBJDIR) $(OBJDIR)/$(LIBNAME)

$(OBJDIR):
	mkdir $(OBJDIR)

$(OBJDIR)/$(LIBNAME): $(OPENAL32_OBJS) $(ALC_OBJS)
	$(CXX) -shared -Xlinker "-soname=$(LIBNAME)" $(OPENAL32_OBJS) $(ALC_OBJS) -lm -lsyllable -o $(OBJDIR)/$(LIBNAME)

install: $(OBJDIR)/$(LIBNAME)
	install -s $(OBJDIR)/$(LIBNAME) $(IMAGE)/$(LIBBIN)/$(LIBNAME)
	ln -sf $(IMAGE)/$(LIBBIN)/$(LIBNAME) $(IMAGE)/$(LIBBIN)/libopenal.so.1
	ln -sf $(IMAGE)/$(LIBBIN)/libAL.so.1 $(IMAGE)/$(LIBBIN)/libopenal.so

dist: install

doc:

install-doc:

deps: $(OBJDIR) $(DEPS)

clean:
	rm -f $(OBJDIR)/*

-include $(OBJDIR)/*.d
