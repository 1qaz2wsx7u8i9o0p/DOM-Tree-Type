// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_LITERAL_BUFFER_H_
#define V8_PARSING_LITERAL_BUFFER_H_

#include <vector>
#include <ctype.h>
#include <string.h>
#include "third_party/blink/renderer/core/frame/v8_scanner/globals.h"

namespace v8_scanner {
// LiteralBuffer -  Collector of chars of literals.
class LiteralBuffer final {
 public:
  LiteralBuffer() : backing_store_(), position_(0), is_one_byte_(true) {}

  ~LiteralBuffer() { backing_store_.clear(); }

  LiteralBuffer(const LiteralBuffer&) = delete;
  LiteralBuffer& operator=(const LiteralBuffer&) = delete;

  void AddChar(char code_unit) {
    AddOneByteChar(static_cast<byte>(code_unit));
  }

  void AddChar(uc32 code_unit) {
    if (is_one_byte()) {
      if (code_unit <= static_cast<uc32>(0xff)) {
        AddOneByteChar(static_cast<byte>(code_unit));
        return;
      }
      ConvertToTwoByte();
    }
    AddTwoByteChar(code_unit);
  }

  bool is_one_byte() const { return is_one_byte_; }

  bool Equals(std::vector<char> keyword) const {
    return is_one_byte() && (int)keyword.size() == position_ &&
           (memcmp(&*keyword.begin(), &*backing_store_.begin(), position_) == 0);
  }

  std::vector<uint16_t> two_byte_literal() const {
    return literal<uint16_t>();
  }

  std::vector<uint8_t> one_byte_literal() const { return literal<uint8_t>(); }

  template <typename Char>
  std::vector<Char> literal() const {
    std::vector<Char> result;
    result.assign(reinterpret_cast<const Char*>(&*backing_store_.begin()), reinterpret_cast<const Char*>(&*backing_store_.begin()) + (position_ >> (sizeof(Char) - 1)));
    return result;
  }

  int length() const { return is_one_byte() ? position_ : (position_ >> 1); }

  void Start() {
    position_ = 0;
    is_one_byte_ = true;
  }

 private:
  static const int kInitialCapacity = 16;
  static const int kGrowthFactor = 4;
  static const int kMaxGrowth = 1 * MB;

  inline bool IsValidAscii(char code_unit) {
    // Control characters and printable characters span the range of
    // valid ASCII characters (0-127). Chars are unsigned on some
    // platforms which causes compiler warnings if the validity check
    // tests the lower bound >= 0 as it's always true.
    return iscntrl(code_unit) || isprint(code_unit);
  }

  void AddOneByteChar(byte one_byte_char) {
    if (position_ >= (int)backing_store_.size()) ExpandBuffer();
    backing_store_[position_] = one_byte_char;
    position_ += kOneByteSize;
  }

  void AddTwoByteChar(uc32 code_unit);
  int NewCapacity(int min_capacity);
  void ExpandBuffer();
  void ConvertToTwoByte();

  std::vector<byte> backing_store_;
  int position_;

  bool is_one_byte_;
};
}
#endif  // V8_PARSING_LITERAL_BUFFER_H_
