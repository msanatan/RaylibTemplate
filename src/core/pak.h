#ifndef GBM_PAK_H
#define GBM_PAK_H

#include <string>

#include "raylib.h"

// Serves every raylib file load under mountPoint from the zip at pakFile
void MountPak(const std::string &pakFile, const std::string &mountPoint);
void UnmountPak();

Music LoadPakMusic(const std::string &entry);
void UnloadPakMusic(Music music);

#endif // !GBM_PAK_H
