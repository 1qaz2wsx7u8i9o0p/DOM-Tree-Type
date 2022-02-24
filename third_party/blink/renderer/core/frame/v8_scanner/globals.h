#ifndef V8_COMMON_GLOBALS_H_
#define V8_COMMON_GLOBALS_H_

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <ostream>

namespace v8_scanner {
constexpr int KB = 1024;
constexpr int MB = KB * 1024;
constexpr int GB = MB * 1024;

using byte = uint8_t;

// -----------------------------------------------------------------------------
// Constants

constexpr int kMaxInt = 0x7FFFFFFF;
constexpr int kMinInt = -kMaxInt - 1;
constexpr int kMaxInt8 = (1 << 7) - 1;
constexpr int kMinInt8 = -(1 << 7);
constexpr int kMaxUInt8 = (1 << 8) - 1;
constexpr int kMinUInt8 = 0;
constexpr int kMaxInt16 = (1 << 15) - 1;
constexpr int kMinInt16 = -(1 << 15);
constexpr int kMaxUInt16 = (1 << 16) - 1;
constexpr int kMinUInt16 = 0;
constexpr int kMaxInt31 = kMaxInt / 2;
constexpr int kMinInt31 = kMinInt / 2;

constexpr uint32_t kMaxUInt32 = 0xFFFFFFFFu;
constexpr int kMinUInt32 = 0;

constexpr int kUInt8Size = sizeof(uint8_t);
constexpr int kByteSize = sizeof(byte);
constexpr int kCharSize = sizeof(char);
constexpr int kShortSize = sizeof(short);  // NOLINT
constexpr int kUInt16Size = sizeof(uint16_t);
constexpr int kIntSize = sizeof(int);
constexpr int kInt32Size = sizeof(int32_t);
constexpr int kInt64Size = sizeof(int64_t);
constexpr int kUInt32Size = sizeof(uint32_t);
constexpr int kSizetSize = sizeof(size_t);
constexpr int kFloatSize = sizeof(float);
constexpr int kDoubleSize = sizeof(double);
constexpr int kIntptrSize = sizeof(intptr_t);
constexpr int kUIntptrSize = sizeof(uintptr_t);
constexpr int kSystemPointerSize = sizeof(void*);
constexpr int kSystemPointerHexDigits = kSystemPointerSize == 4 ? 8 : 12;
constexpr int kPCOnStackSize = kSystemPointerSize;
constexpr int kFPOnStackSize = kSystemPointerSize;

enum class LanguageMode : bool { kSloppy, kStrict };

inline bool is_sloppy(LanguageMode language_mode) {
  return language_mode == LanguageMode::kSloppy;
}

using uc16 = uint16_t;
using uc32 = uint32_t;
constexpr int kOneByteSize = kCharSize;
constexpr int kUC16Size = sizeof(uc16);  // NOLINT
}
#endif  // V8_COMMON_GLOBALS_H_
