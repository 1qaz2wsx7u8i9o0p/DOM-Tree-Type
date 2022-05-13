#ifndef V8_UTILS_UTILS_H_
#define V8_UTILS_UTILS_H_

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <cmath>
#include <string>
#include <type_traits>

#include "third_party/blink/renderer/core/frame/v8_scanner/globals.h"
#include "third_party/blink/renderer/core/frame/v8_scanner/bounds.h"
#include "third_party/blink/renderer/core/frame/v8_scanner/unicode.h"

namespace v8_scanner {
// clang-format off
#define INT_0_TO_127_LIST(V)                                          \
V(0)   V(1)   V(2)   V(3)   V(4)   V(5)   V(6)   V(7)   V(8)   V(9)   \
V(10)  V(11)  V(12)  V(13)  V(14)  V(15)  V(16)  V(17)  V(18)  V(19)  \
V(20)  V(21)  V(22)  V(23)  V(24)  V(25)  V(26)  V(27)  V(28)  V(29)  \
V(30)  V(31)  V(32)  V(33)  V(34)  V(35)  V(36)  V(37)  V(38)  V(39)  \
V(40)  V(41)  V(42)  V(43)  V(44)  V(45)  V(46)  V(47)  V(48)  V(49)  \
V(50)  V(51)  V(52)  V(53)  V(54)  V(55)  V(56)  V(57)  V(58)  V(59)  \
V(60)  V(61)  V(62)  V(63)  V(64)  V(65)  V(66)  V(67)  V(68)  V(69)  \
V(70)  V(71)  V(72)  V(73)  V(74)  V(75)  V(76)  V(77)  V(78)  V(79)  \
V(80)  V(81)  V(82)  V(83)  V(84)  V(85)  V(86)  V(87)  V(88)  V(89)  \
V(90)  V(91)  V(92)  V(93)  V(94)  V(95)  V(96)  V(97)  V(98)  V(99)  \
V(100) V(101) V(102) V(103) V(104) V(105) V(106) V(107) V(108) V(109) \
V(110) V(111) V(112) V(113) V(114) V(115) V(116) V(117) V(118) V(119) \
V(120) V(121) V(122) V(123) V(124) V(125) V(126) V(127)
// clang-format on

// Constexpr cache table for character flags.
enum OneByteCharFlags {
  kIsIdentifierStart = 1 << 0,
  kIsIdentifierPart = 1 << 1,
  kIsWhiteSpace = 1 << 2,
  kIsWhiteSpaceOrLineTerminator = 1 << 3,
  kMaybeLineEnd = 1 << 4
};

// See http://www.unicode.org/Public/UCD/latest/ucd/DerivedCoreProperties.txt
// ID_Start. Additionally includes '_' and '$'.
constexpr bool IsOneByteIDStart(uc32 c) {
  return c == 0x0024 || (c >= 0x0041 && c <= 0x005A) || c == 0x005F ||
         (c >= 0x0061 && c <= 0x007A) || c == 0x00AA || c == 0x00B5 ||
         c == 0x00BA || (c >= 0x00C0 && c <= 0x00D6) ||
         (c >= 0x00D8 && c <= 0x00F6) || (c >= 0x00F8 && c <= 0x00FF);
}

// See http://www.unicode.org/Public/UCD/latest/ucd/DerivedCoreProperties.txt
// ID_Continue. Additionally includes '_' and '$'.
constexpr bool IsOneByteIDContinue(uc32 c) {
  return c == 0x0024 || (c >= 0x0030 && c <= 0x0039) || c == 0x005F ||
         (c >= 0x0041 && c <= 0x005A) || (c >= 0x0061 && c <= 0x007A) ||
         c == 0x00AA || c == 0x00B5 || c == 0x00B7 || c == 0x00BA ||
         (c >= 0x00C0 && c <= 0x00D6) || (c >= 0x00D8 && c <= 0x00F6) ||
         (c >= 0x00F8 && c <= 0x00FF);
}

constexpr bool IsOneByteWhitespace(uc32 c) {
  return c == '\t' || c == '\v' || c == '\f' || c == ' ' || c == u'\xa0';
}

constexpr uint8_t BuildOneByteCharFlags(uc32 c) {
  uint8_t result = 0;
  if (IsOneByteIDStart(c) || c == '\\') result |= kIsIdentifierStart;
  if (IsOneByteIDContinue(c) || c == '\\') result |= kIsIdentifierPart;
  if (IsOneByteWhitespace(c)) {
    result |= kIsWhiteSpace | kIsWhiteSpaceOrLineTerminator;
  }
  if (c == '\r' || c == '\n') {
    result |= kIsWhiteSpaceOrLineTerminator | kMaybeLineEnd;
  }
  // Add markers to identify 0x2028 and 0x2029.
  if (c == static_cast<uint8_t>(0x2028) || c == static_cast<uint8_t>(0x2029)) {
    result |= kMaybeLineEnd;
  }
  return result;
}

const constexpr uint8_t kOneByteCharFlags[256] = {
#define BUILD_CHAR_FLAGS(N) BuildOneByteCharFlags(N),
    INT_0_TO_127_LIST(BUILD_CHAR_FLAGS)
#undef BUILD_CHAR_FLAGS
#define BUILD_CHAR_FLAGS(N) BuildOneByteCharFlags(N + 128),
        INT_0_TO_127_LIST(BUILD_CHAR_FLAGS)
#undef BUILD_CHAR_FLAGS
};

inline bool IsIdentifierStartSlow(uc32 c) {
  // Non-BMP characters are not supported without I18N.
  return (c <= 0xFFFF) ? unibrow::ID_Start::Is(c) : false;
}

bool IsIdentifierStart(uc32 c) {
  if (!IsInRange(c, 0, 255)) return IsIdentifierStartSlow(c);
  return kOneByteCharFlags[c] & kIsIdentifierStart;
}

inline bool IsIdentifierPartSlow(uc32 c) {
  // Non-BMP characters are not supported without I18N.
  if (c <= 0xFFFF) {
    return unibrow::ID_Start::Is(c) || unibrow::ID_Continue::Is(c);
  }
  return false;
}

bool IsIdentifierPart(uc32 c) {
  if (!IsInRange(c, 0, 255)) return IsIdentifierPartSlow(c);
  return kOneByteCharFlags[c] & kIsIdentifierPart;
}

inline int HexValue(uc32 c) {
  c -= '0';
  if (static_cast<unsigned>(c) <= 9) return c;
  c = (c | 0x20) - ('a' - '0');  // detect 0x11..0x16 and 0x31..0x36.
  if (static_cast<unsigned>(c) <= 5) return c + 10;
  return -1;
}

inline bool IsWhiteSpaceSlow(uc32 c) { return unibrow::WhiteSpace::Is(c); }

bool IsWhiteSpace(uc32 c) {
  if (IsInRange(c, 0, 255)) return IsWhiteSpaceSlow(c);
  return kOneByteCharFlags[c] & kIsWhiteSpace;
}

inline bool IsWhiteSpaceOrLineTerminatorSlow(uc32 c) {
  return IsWhiteSpaceSlow(c) || unibrow::IsLineTerminator(c);
}

bool IsWhiteSpaceOrLineTerminator(uc32 c) {
  if (!IsInRange(c, 0, 255)) return IsWhiteSpaceOrLineTerminatorSlow(c);
  return kOneByteCharFlags[c] & kIsWhiteSpaceOrLineTerminator;
}

inline constexpr int AsciiAlphaToLower(uc32 c) { return c | 0x20; }

inline constexpr bool IsCarriageReturn(uc32 c) { return c == 0x000D; }

inline constexpr bool IsLineFeed(uc32 c) { return c == 0x000A; }

inline constexpr bool IsDecimalDigit(uc32 c) {
  // ECMA-262, 3rd, 7.8.3 (p 16)
  return IsInRange(c, '0', '9');
}

inline constexpr bool IsAlphaNumeric(uc32 c) {
  return IsInRange(AsciiAlphaToLower(c), 'a', 'z') || IsDecimalDigit(c);
}

inline constexpr bool IsAsciiIdentifier(uc32 c) {
  return IsAlphaNumeric(c) || c == '$' || c == '_';
}

inline constexpr bool IsHexDigit(uc32 c) {
  // ECMA-262, 3rd, 7.6 (p 15)
  return IsDecimalDigit(c) || IsInRange(AsciiAlphaToLower(c), 'a', 'f');
}

inline constexpr bool IsOctalDigit(uc32 c) {
  // ECMA-262, 6th, 7.8.3
  return IsInRange(c, '0', '7');
}

inline constexpr bool IsNonOctalDecimalDigit(uc32 c) {
  return IsInRange(c, '8', '9');
}

inline constexpr bool IsBinaryDigit(uc32 c) {
  // ECMA-262, 6th, 7.8.3
  return c == '0' || c == '1';
}

inline constexpr bool IsAsciiLower(uc32 c) {
  return IsInRange(c, 'a', 'z');
}

inline constexpr bool IsAsciiUpper(uc32 c) {
  return IsInRange(c, 'A', 'Z');
}

inline constexpr uc32 ToAsciiUpper(uc32 c) {
  return c & ~(IsAsciiLower(c) << 5);
}

inline constexpr uc32 ToAsciiLower(uc32 c) {
  return c | (IsAsciiUpper(c) << 5);
}

inline constexpr bool IsRegExpWord(uc32 c) {
  return IsAlphaNumeric(c) || c == '_';
}

template <typename T, size_t N>
char (&ArraySizeHelper(T (&array)[N]))[N];

template <typename T, size_t N>
char (&ArraySizeHelper(const T (&array)[N]))[N];

#define arraysize(array) (sizeof(ArraySizeHelper(array)))
}
#endif  // V8_UTILS_UTILS_H_
