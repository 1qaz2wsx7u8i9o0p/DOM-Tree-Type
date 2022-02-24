// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/v8_scanner/scanner-character-streams.h"

#include <memory>
#include <vector>

#include "third_party/blink/renderer/core/frame/v8_scanner/globals.h"
#include "third_party/blink/renderer/core/frame/v8_scanner/scanner.h"
#include "third_party/blink/renderer/core/frame/v8_scanner/unicode-inl.h"

namespace v8_scanner {

template <typename Char>
struct Range {
  const Char* start;
  const Char* end;

  size_t length() { return static_cast<size_t>(end - start); }
  bool unaligned_start() const {
    return reinterpret_cast<intptr_t>(start) % sizeof(Char) == 1;
  }
};

// A Char stream backed by a C array. Testing only.
template <typename Char>
class TestingStream {
 public:
  TestingStream(const Char* data, size_t length)
      : data_(data), length_(length) {}
  // The no_gc argument is only here because of the templated way this class
  // is used along with other implementations that require V8 heap access.
  Range<Char> GetDataAt(size_t pos) {
    return {&data_[std::min(length_, pos)], &data_[length_]};
  }

  static const bool kCanBeCloned = true;
  static const bool kCanAccessHeap = false;

 private:
  const Char* const data_;
  const size_t length_;
};

// Provides a buffered utf-16 view on the bytes from the underlying ByteStream.
// Chars are buffered if either the underlying stream isn't utf-16 or the
// underlying utf-16 stream might move (is on-heap).
template <template <typename T> class ByteStream>
class BufferedCharacterStream : public Utf16CharacterStream {
 public:
  template <class... TArgs>
  BufferedCharacterStream(size_t pos, TArgs... args) : byte_stream_(args...) {
    buffer_pos_ = pos;
  }

  bool can_be_cloned() const final {
    return ByteStream<uint16_t>::kCanBeCloned;
  }

  std::unique_ptr<Utf16CharacterStream> Clone() const override {
    return std::unique_ptr<Utf16CharacterStream>(
        new BufferedCharacterStream<ByteStream>(*this));
  }

 protected:
  bool ReadBlock() final {
    size_t position = pos();
    buffer_pos_ = position;
    buffer_start_ = &buffer_[0];
    buffer_cursor_ = buffer_start_;

    Range<uint8_t> range =
        byte_stream_.GetDataAt(position);
    if (range.length() == 0) {
      buffer_end_ = buffer_start_;
      return false;
    }

    size_t length = std::min({kBufferSize, range.length()});
    for (unsigned int i = 0; i < length; ++i) {
      buffer_[i] = *((uc16 *)(range.start) + i);
    }
    buffer_end_ = &buffer_[length];
    return true;
  }

  bool can_access_heap() const final {
    return ByteStream<uint8_t>::kCanAccessHeap;
  }

 private:
  BufferedCharacterStream(const BufferedCharacterStream<ByteStream>& other)
      : byte_stream_(other.byte_stream_) {}

  static const size_t kBufferSize = 512;
  uc16 buffer_[kBufferSize];
  ByteStream<uint8_t> byte_stream_;
};

// Provides a unbuffered utf-16 view on the bytes from the underlying
// ByteStream.
template <template <typename T> class ByteStream>
class UnbufferedCharacterStream : public Utf16CharacterStream {
 public:
  template <class... TArgs>
  UnbufferedCharacterStream(size_t pos, TArgs... args) : byte_stream_(args...) {
    buffer_pos_ = pos;
  }

  bool can_access_heap() const final {
    return ByteStream<uint16_t>::kCanAccessHeap;
  }

  bool can_be_cloned() const final {
    return ByteStream<uint16_t>::kCanBeCloned;
  }

  std::unique_ptr<Utf16CharacterStream> Clone() const override {
    return std::unique_ptr<Utf16CharacterStream>(
        new UnbufferedCharacterStream<ByteStream>(*this));
  }

 protected:
  bool ReadBlock() final {
    size_t position = pos();
    buffer_pos_ = position;
    Range<uint16_t> range =
        byte_stream_.GetDataAt(position);
    buffer_start_ = range.start;
    buffer_end_ = range.end;
    buffer_cursor_ = buffer_start_;
    if (range.length() == 0) return false;

    return true;
  }

  UnbufferedCharacterStream(const UnbufferedCharacterStream<ByteStream>& other)
      : byte_stream_(other.byte_stream_) {}

  ByteStream<uint16_t> byte_stream_;
};

// ----------------------------------------------------------------------------
// BufferedUtf16CharacterStreams
//
// A buffered character stream based on a random access character
// source (ReadBlock can be called with pos() pointing to any position,
// even positions before the current).
//
// TODO(verwaest): Remove together with Utf8 external streaming streams.
class BufferedUtf16CharacterStream : public Utf16CharacterStream {
 public:
  BufferedUtf16CharacterStream();

 protected:
  static const size_t kBufferSize = 512;

  bool ReadBlock() final;

  // FillBuffer should read up to kBufferSize characters at position and store
  // them into buffer_[0..]. It returns the number of characters stored.
  virtual size_t FillBuffer(size_t position) = 0;

  // Fixed sized buffer that this class reads from.
  // The base class' buffer_start_ should always point to buffer_.
  uc16 buffer_[kBufferSize];
};

BufferedUtf16CharacterStream::BufferedUtf16CharacterStream()
    : Utf16CharacterStream(buffer_, buffer_, buffer_, 0) {}

bool BufferedUtf16CharacterStream::ReadBlock() {
  size_t position = pos();
  buffer_pos_ = position;
  buffer_cursor_ = buffer_;
  buffer_end_ = buffer_ + FillBuffer(position);
  return buffer_cursor_ < buffer_end_;
}

std::unique_ptr<Utf16CharacterStream> ScannerStream::ForTesting(
    const char* data) {
  return ScannerStream::ForTesting(data, strlen(data));
}

std::unique_ptr<Utf16CharacterStream> ScannerStream::ForTesting(
    const char* data, size_t length) {
  if (data == nullptr) {
    // We don't want to pass in a null pointer into the the character stream,
    // because then the one-past-the-end pointer is undefined, so instead pass
    // through this static array.
    static const char non_null_empty_string[1] = {0};
    data = non_null_empty_string;
  }

  return std::unique_ptr<Utf16CharacterStream>(
      new BufferedCharacterStream<TestingStream>(
          0, reinterpret_cast<const uint8_t*>(data), length));
}

std::unique_ptr<Utf16CharacterStream> ScannerStream::ForTesting(
    const uint16_t* data, size_t length) {
  if (data == nullptr) {
    // We don't want to pass in a null pointer into the the character stream,
    // because then the one-past-the-end pointer is undefined, so instead pass
    // through this static array.
    static const uint16_t non_null_empty_uint16_t_string[1] = {0};
    data = non_null_empty_uint16_t_string;
  }

  return std::unique_ptr<Utf16CharacterStream>(
      new UnbufferedCharacterStream<TestingStream>(0, data, length));
}
}
