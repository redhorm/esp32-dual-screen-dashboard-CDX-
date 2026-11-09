#pragma once
#include "storm_state.h"
#include <stddef.h>
namespace Storm {
struct Archive {
  uint32_t magic = 0x53544431, version = 3, count = 0, best = 0, weight = 5;
  Result results[64];
  uint64_t testMask = 0;
  uint32_t checksum = 0;
};
inline uint32_t archiveChecksum(const Archive &a) {
  const auto p = reinterpret_cast<const uint8_t *>(&a);
  uint32_t h = 2166136261;
  for (size_t i = 0; i < offsetof(Archive, checksum); ++i) {
    h ^= p[i];
    h *= 16777619;
  }
  return h;
}
inline bool validArchive(const Archive &a) {
  return a.magic == 0x53544431 && a.version == 3 && a.count <= 64 &&
         a.weight <= 100 && a.checksum == archiveChecksum(a);
}
inline void appendResult(Archive &a, const Result &r, bool test = false) {
  if (a.count == 64) {
    for (int i = 1; i < 64; ++i)
      a.results[i - 1] = a.results[i];
    a.count = 63;
    a.testMask >>= 1;
  }
  const uint64_t bit = uint64_t(1) << a.count;
  a.testMask = test ? (a.testMask | bit) : (a.testMask & ~bit);
  a.results[a.count++] = r;
}
} // namespace Storm
