CC = cc
CXX = g++
ECC = emcc
CFLAGS = -std=c++11
ECCFLAGS = -Os -Wall
LDFLAGS_MAC = -framework IOKit -framework Cocoa -framework OpenGL `pkg-config --libs raylib`
LDFLAGS_MAC_CI = -framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL libraylib.a
LDFLAGS_WIN = -lraylib -lopengl32 -lgdi32 -lwinmm -mwindows
INCFLAGS_WIN = -IC:/raylib/raylib/src
INCFLAGS_CI_WIN = -I${WORKSPACE_DIR}/raylib/include -L${WORKSPACE_DIR}/raylib/lib
INCFLAGS_CI_WEB = ${WORKSPACE_DIR}/raylib/lib/libraylib.a -I${WORKSPACE_DIR}/src -I${WORKSPACE_DIR}/raylib/include -L${WORKSPACE_DIR}/raylib/lib

SRC = src/*.cpp
BUILDDIR = build
APP_NAME = $(shell basename "$(CURDIR)")
export APP_NAME
TARGET_MAC = $(BUILDDIR)/$(APP_NAME).app
TARGET_WIN = $(BUILDDIR)/$(APP_NAME).exe
TARGET_WEB = $(BUILDDIR)/index.html

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

build-mac: $(BUILDDIR)
	$(CC) $(CFLAGS) $(LDFLAGS_MAC) $(SRC) -o $(TARGET_MAC)

build-mac-ci: $(BUILDDIR)
	$(CC) $(CFLAGS) $(LDFLAGS_MAC_CI) $(SRC) -o $(TARGET_MAC)

build-win: $(BUILDDIR)
	$(CXX) $(CFLAGS) -static-libgcc -static-libstdc++ $(INCFLAGS_WIN) $(SRC) -o $(TARGET_WIN) $(LDFLAGS_WIN)

build-win-ci: $(BUILDDIR)
	$(CXX) $(CFLAGS) -static-libgcc -static-libstdc++ -static -lpthread $(INCFLAGS_CI_WIN) $(SRC) -o $(TARGET_WIN) $(LDFLAGS_WIN)

build-web: $(BUILDDIR)
	$(ECC) -o $(TARGET_WEB) $(SRC) $(ECCFLAGS) $(INCFLAGS_CI_WEB) -s USE_GLFW=3 -DPLATFORM_WEB

run-mac:
	$(TARGET_MAC)

run-win:
	$(TARGET_WIN)

.PHONY: clean
clean:
	rm -rf $(BUILDDIR)/*
