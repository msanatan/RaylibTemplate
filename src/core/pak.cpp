#include "core/pak.h"

#include <cstring>
#include <filesystem>
#include <unordered_map>

#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "miniz.h"

namespace {
mz_zip_archive pak = {};
std::string mountPoint;
std::unordered_map<void*, unsigned char*> musicBuffers;

unsigned char* ReadEntry(const std::string& entry, int* dataSize) {
  // glTF files can reference "../textures/x.png", but zip entry names are
  // matched literally
  const std::string name =
      std::filesystem::path(entry).lexically_normal().generic_string();
  const int index = mz_zip_reader_locate_file(&pak, name.c_str(), nullptr, 0);
  mz_zip_archive_file_stat stat;
  if (index < 0 || !mz_zip_reader_file_stat(&pak, (mz_uint)index, &stat)) {
    TraceLog(LOG_WARNING, "PAK: [%s] Not found", name.c_str());
    return nullptr;
  }

  // One spare zeroed byte, so the same buffer works for raylib's text loads
  const size_t size = (size_t)stat.m_uncomp_size;
  unsigned char* data = (unsigned char*)MemAlloc((unsigned int)size + 1);
  if (!mz_zip_reader_extract_to_mem(&pak, (mz_uint)index, data, size, 0)) {
    MemFree(data);
    return nullptr;
  }
  *dataSize = (int)size;
  return data;
}

bool IsInPak(const char* fileName) {
  return std::strncmp(fileName, mountPoint.c_str(), mountPoint.size()) == 0;
}

unsigned char* LoadData(const char* fileName, int* dataSize) {
  if (IsInPak(fileName)) {
    return ReadEntry(fileName + mountPoint.size(), dataSize);
  }
  // Saves and other files outside the pak need raylib's disk loader, which only
  // runs with the callback removed
  SetLoadFileDataCallback(nullptr);
  unsigned char* data = LoadFileData(fileName, dataSize);
  SetLoadFileDataCallback(LoadData);
  return data;
}

char* LoadText(const char* fileName) {
  if (IsInPak(fileName)) {
    int dataSize = 0;
    return (char*)ReadEntry(fileName + mountPoint.size(), &dataSize);
  }
  SetLoadFileTextCallback(nullptr);
  char* text = LoadFileText(fileName);
  SetLoadFileTextCallback(LoadText);
  return text;
}
}  // namespace

void MountPak(const std::string& pakFile, const std::string& pakMountPoint) {
  mountPoint = pakMountPoint;
  if (!mz_zip_reader_init_file(&pak, pakFile.c_str(), 0)) {
    TraceLog(LOG_ERROR, "PAK: Failed to open [%s]", pakFile.c_str());
  }
  SetLoadFileDataCallback(LoadData);
  SetLoadFileTextCallback(LoadText);
}

void UnmountPak() {
  SetLoadFileDataCallback(nullptr);
  SetLoadFileTextCallback(nullptr);
  mz_zip_reader_end(&pak);
}

Music LoadPakMusic(const std::string& entry) {
  int dataSize = 0;
  unsigned char* data = ReadEntry(entry, &dataSize);
  if (data == nullptr) {
    return {};
  }

  const std::string fileType = TextToLower(GetFileExtension(entry.c_str()));
  const Music music =
      LoadMusicStreamFromMemory(fileType.c_str(), data, dataSize);
  if (!IsMusicValid(music)) {
    MemFree(data);
    return music;
  }
  // The stream decodes from this buffer until it is unloaded
  musicBuffers[music.ctxData] = data;
  return music;
}

void UnloadPakMusic(Music music) {
  UnloadMusicStream(music);
  MemFree(musicBuffers[music.ctxData]);
  musicBuffers.erase(music.ctxData);
}
