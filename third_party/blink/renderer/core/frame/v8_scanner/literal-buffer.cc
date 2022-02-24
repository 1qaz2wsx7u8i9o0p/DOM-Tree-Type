// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/v8_scanner/literal-buffer.h"
#include "third_party/blink/renderer/core/frame/v8_scanner/unicode.h"

// #include "src/execution/isolate.h"
// #include "src/execution/local-isolate.h"
// #include "src/heap/factory.h"
// #include "src/utils/memcopy.h"

namespace v8_scanner {

int LiteralBuffer::NewCapacity(int min_capacity) {
  return min_capacity < (kMaxGrowth / (kGrowthFactor - 1))
             ? min_capacity * kGrowthFactor
             : min_capacity + kMaxGrowth;
}

void LiteralBuffer::ExpandBuffer() {
  int min_capacity = kInitialCapacity > backing_store_.size() ? kInitialCapacity : backing_store_.size();
  std::vector<byte> new_store = std::vector<byte>(NewCapacity(min_capacity));
  if (position_ > 0) {
    memcpy(&*new_store.begin(), &*backing_store_.begin(), position_);
  }
  backing_store_.clear();
  backing_store_ = new_store;
}

void LiteralBuffer::ConvertToTwoByte() {
  std::vector<byte> new_store;
  int new_content_size = position_ * kUC16Size;
  if (new_content_size >= (int)backing_store_.size()) {
    // Ensure room for all currently read code units as UC16 as well
    // as the code unit about to be stored.
    new_store = std::vector<byte>(NewCapacity(new_content_size));
  } else {
    new_store = backing_store_;
  }
  uint8_t* src = &*backing_store_.begin();
  uint16_t* dst = reinterpret_cast<uint16_t*>(&*new_store.begin());
  for (int i = position_ - 1; i >= 0; i--) {
    dst[i] = src[i];
  }
  if (new_store.begin() != backing_store_.begin()) {
    backing_store_.clear();
    backing_store_ = new_store;
  }
  position_ = new_content_size;
  is_one_byte_ = false;
}

void LiteralBuffer::AddTwoByteChar(uc32 code_unit) {
  if (position_ >= (int)backing_store_.size()) ExpandBuffer();
  if (code_unit <=
      static_cast<uc32>(unibrow::Utf16::kMaxNonSurrogateCharCode)) {
    *reinterpret_cast<uint16_t*>(&backing_store_[position_]) = code_unit;
    position_ += kUC16Size;
  } else {
    *reinterpret_cast<uint16_t*>(&backing_store_[position_]) =
        unibrow::Utf16::LeadSurrogate(code_unit);
    position_ += kUC16Size;
    if (position_ >= (int)backing_store_.size()) ExpandBuffer();
    *reinterpret_cast<uint16_t*>(&backing_store_[position_]) =
        unibrow::Utf16::TrailSurrogate(code_unit);
    position_ += kUC16Size;
  }
}
}
