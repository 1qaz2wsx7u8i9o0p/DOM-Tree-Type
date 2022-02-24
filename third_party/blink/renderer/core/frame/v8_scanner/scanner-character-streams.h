// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_SCANNER_CHARACTER_STREAMS_H_
#define V8_PARSING_SCANNER_CHARACTER_STREAMS_H_

#include <memory>

namespace v8_scanner {
template <typename T>
class Handle;
class Utf16CharacterStream;
class RuntimeCallStats;
class String;

class ScannerStream {
 public:
  static std::unique_ptr<Utf16CharacterStream> ForTesting(const char* data);
  static std::unique_ptr<Utf16CharacterStream> ForTesting(const char* data,
                                                          size_t length);
  static std::unique_ptr<Utf16CharacterStream> ForTesting(const uint16_t* data,
                                                        size_t length);
};
}
#endif  // V8_PARSING_SCANNER_CHARACTER_STREAMS_H_
