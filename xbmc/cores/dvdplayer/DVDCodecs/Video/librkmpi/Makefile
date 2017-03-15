ARCH=arm

INCLUDES+=-I/home2/lyh/RKMC/xbmc/xbmc/cores/dvdplayer
INCLUDES+=-I..

SRCS += MpiWrapper.cpp
SRCS += MpiVideoPrivate.cpp
INCLUDES += -I${prefix}/opt/android-source/frameworks/base/include
INCLUDES += -I${prefix}/opt/android-source/frameworks/base/native/include
INCLUDES += -I${prefix}/opt/android-source/system/core/include
INCLUDES += -I${prefix}/opt/android-source/libhardware/include

LIBNAME=libRKvcodec_mpi
LIB_SHARED=/home2/lyh/RKMC/xbmc/system/players/dvdplayer/$(LIBNAME)-$(ARCH).so

LIBS += -landroid -lEGL -lGLESv2 -L${prefix}/opt/android-libs -lstdc++ -lutils -lcutils -lbinder -lui -lgui -L/home2/lyh/RKMC/xbmc -lkodi

all: $(LIB_SHARED)

include /home2/lyh/RKMC/xbmc/Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))

JNICXXFLAGS = $(CXXFLAGS) -std=gnu++0x -Wextra
MPICXXFLAGS = $(CXXFLAGS) -Wno-multichar -fno-rtti -fPIC

$(LIB_SHARED): $(OBJS)
	$(CXX) $(MPICXXFLAGS) $(LDFLAGS) -shared -Wl,-no-undefined -g -o $(LIB_SHARED) $(OBJS) $(LIBS)

MpiWrapper.o: MpiWrapper.cpp
	$(CXX) -MF $*.d -MD -c $(MPICXXFLAGS) $(DEFINES) $(INCLUDES) $< -o $@

MpiVideoPrivate.o: MpiVideoPrivate.cpp
	$(CXX) -MF $*.d -MD -c $(MPICXXFLAGS) $(DEFINES) $(INCLUDES) $< -o $@

Surface.o: Surface.cpp
	$(CXX) -MF $*.d -MD -c $(JNICXXFLAGS) $(DEFINES) $(INCLUDES) $< -o $@

SurfaceTexture.o: SurfaceTexture.cpp
	$(CXX) -MF $*.d -MD -c $(JNICXXFLAGS) $(DEFINES) $(INCLUDES) $< -o $@

SurfaceComposerClient.o: SurfaceComposerClient.cpp
	$(CXX) -MF $*.d -MD -c $(JNICXXFLAGS) $(DEFINES) $(INCLUDES) $< -o $@

CLEAN_FILES = \
        $(LIB_SHARED) \

DISTCLEAN_FILES= \
        Makefile \
