#ifndef FILE_UTILS_H
#define FILE_UTILS_H

namespace fallout {

int fileCopyDecompressed(const char* existingFilePath, const char* newFilePath);
// Capture exact bytes only when the entire successfully restored file fits.
int fileCopyDecompressedSnapshot(const char* source, const char* destination, unsigned char* snapshot, unsigned int capacity, unsigned int* size);
// Bounded read of a raw or gzip record without creating a destination.
// 0 = complete record; 1 = exceeds capacity; -1 = read/close error.
int fileReadSmallDecompressed(const char* source, unsigned char* bytes, unsigned int capacity, unsigned int* size);
// Enumerate stock patchNNN.dat names in one directory pass; false requests legacy fallback.
bool fileFindStockPatches(bool* present);
int fileCopyCompressed(const char* existingFilePath, const char* newFilePath);
// Delete only loose files in the given directory. Returns -2 if native
// enumeration could not be used (caller may fall back), -1 on deletion error.
int fileEraseLocalExtension(const char* directory, const char* extension);
int _gzdecompress_file(const char* existingFilePath, const char* newFilePath);

} // namespace fallout

#endif /* FILE_UTILS_H */
