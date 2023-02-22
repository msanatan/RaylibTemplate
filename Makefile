CC = cc
CXX = g++
CFLAGS = -std=c++11
LDFLAGS_MAC = -framework IOKit -framework Cocoa -framework OpenGL `pkg-config --libs raylib`
LDFLAGS_MAC_CI = -framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL libraylib.a
LDFLAGS_WIN = -lraylib -lopengl32 -lgdi32 -lwinmm -mwindows
INCFLAGS_WIN = -I${GITHUB_WORKSPACE}\raylib\include -L${GITHUB_WORKSPACE}\raylib\lib
SRC_MAC = src/main.cpp
SRC_WIN = src\main.cpp
BUILDDIR = build
TARGET_MAC = $(BUILDDIR)/MoveSquare.app
TARGET_WIN = $(BUILDDIR)\MoveSquare.exe

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

build-mac: $(BUILDDIR)
	$(CC) $(CFLAGS) $(LDFLAGS_MAC) $(SRC_MAC) -o $(TARGET_MAC)

build-mac-ci: $(BUILDDIR)
	$(CC) $(CFLAGS) $(LDFLAGS_MAC_CI) $(SRC_MAC) -o $(TARGET_MAC)

build-win: $(BUILDDIR)
	$(CXX) $(CFLAGS) -static-libgcc -static-libstdc++ $(SRC_WIN) -o $(TARGET_WIN) $(LDFLAGS_WIN)

build-win-ci: $(BUILDDIR)
	$(CXX) $(CFLAGS) -static-libgcc -static-libstdc++ -static -lpthread $(INCFLAGS_WIN) $(SRC_WIN) -o $(TARGET_WIN) $(LDFLAGS_WIN)

run-mac:
	$(TARGET_MAC)

run-win:
	$(TARGET_WIN)

.PHONY: clean
clean:
	rm -rf $(BUILDDIR)/*
