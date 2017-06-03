.PHONY: clean

SOURCES := $(wildcard src/*.cpp)
HEADERS := $(wildcard include/*.h)
OBJECTS := $(SOURCES:src/%.cpp=build/%.o)

CXXFLAGS := -g

INCLUDE_FFMPEG := $(shell pkg-config --cflags \
    libavformat libavcodec libswscale)

LINK_FFMPEG := $(shell pkg-config --libs \
    libavformat libavcodec libswscale)

INCLUDE_GLFW3 := $(shell pkg-config --cflags \
    glfw3)

LINK_GLFW3 := $(shell pkg-config --libs --static \
	glfw3) -lGL

video_sphere : $(OBJECTS) Makefile
	@echo "Linking: $@"
	@g++ -o $@ $(OBJECTS) $(LINK_FFMPEG) $(LINK_GLFW3)

build/%.o : src/%.cpp $(HEADERS) Makefile
	@echo "Compiling C++: $<"
	@mkdir -p ./build
	@g++ -Iinclude -o $@ $(CXXFLAGS) -c $< $(INCLUDE_FFMPEG) $(INCLUDE_GLFW3) -w

clean:
	@rm -rf build
	@rm -f video_sphere
