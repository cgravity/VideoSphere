.PHONY: clean

SOURCES := $(wildcard src/*.cpp)
HEADERS := $(wildcard include/*.h)
OBJECTS := $(SOURCES:src/%.cpp=build/%.o)

CXXFLAGS := -w -g -D__STDC_CONSTANT_MACROS -O3

-include local.mk

INCLUDE_FFMPEG ?= $(shell pkg-config --cflags \
    libavformat libavcodec libswscale)

LINK_FFMPEG ?= $(shell pkg-config --libs \
    libavformat libavcodec libswscale)

INCLUDE_GLFW3 ?= $(shell pkg-config --cflags \
    glfw3)

LINK_GLFW3 ?= $(shell pkg-config --libs --static \
	glfw3) -lGL

INCLUDE_GLEW ?=  $(shell pkg-config --cflags glew)

LINK_GLEW ?= $(shell pkg-config --libs glew)

INCLUDE_PA ?= $(shell pkg-config --cflags portaudio-2.0)
LINK_PA ?= $(shell pkg-config --libs portaudio-2.0)

video_sphere : build/oscpack_1_1_0/liboscpack.so.1.1.0 $(OBJECTS) Makefile
	@echo "Linking: $@"
	@g++ -o $@ $(OBJECTS) $(LINK_FFMPEG) $(LINK_GLFW3) $(LINK_GLEW) -Lbuild/oscpack_1_1_0 -loscpack -Wl,-rpath=build/oscpack_1_1_0 $(LINK_PA)

build/%.o : src/%.cpp $(HEADERS) Makefile
	@echo "Compiling C++: $<"
	@mkdir -p ./build
	@g++ -Iinclude -Ibuild -o $@ $(CXXFLAGS) -c $< $(INCLUDE_FFMPEG) $(INCLUDE_GLFW3) \
	    $(INCLUDE_GLEW) $(INCLUDE_PA)

build/oscpack_1_1_0/liboscpack.so.1.1.0 : oscpack_1_1_0.zip
	@echo "Building OSCPack 1.1.0..."
	@unzip oscpack_1_1_0.zip -d build/
	@cd build/oscpack_1_1_0/ && make clean
	@cd build/oscpack_1_1_0/ &&  sed -i 's/ENDIANESS=OSC_DETECT_ENDIANESS/ENDIANESS=OSC_HOST_LITTLE_ENDIAN/' Makefile
	@cd build/oscpack_1_1_0/ && sed -i 's/CXXFLAGS :=/CXXFLAGS := -fPIC/' Makefile
	@cd build/oscpack_1_1_0 && make lib
	@cd build/oscpack_1_1_0 && ln -s liboscpack.so.1.1.0 liboscpack.so


clean:
	@rm -rf build
	@rm -f video_sphere
