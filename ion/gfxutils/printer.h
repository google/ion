/**
Copyright 2017 Google Inc. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS-IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*/

#ifndef ION_GFXUTILS_PRINTER_H_
#define ION_GFXUTILS_PRINTER_H_

#include <functional>
#include <iostream>  // NOLINT

#include "ion/gfx/node.h"

namespace ion {
namespace gfxutils {

// The Printer class can be used for debugging. It prints ION scene graphs to
// a stream.
class ION_API Printer {
 public:
  // Available output formats.
  // 
  enum Format {
    kText,   // Regular text format.
    kHtml,   // HTML format (a tree of nested objects).
  };
  Printer();
  ~Printer();

  // Sets/returns the printed format. The default is kText.
  void SetFormat(Format format) { format_ = format; }
  Format GetFormat() const { return format_; }

  // Sets/returns a flag indicating whether shape contents should be written.
  // The default is false.
  void EnableFullShapePrinting(bool enable) {
    full_shape_printing_enabled_ = enable;
  }
  bool IsFullShapePrintingEnabled() const {
    return full_shape_printing_enabled_;
  }

  // Sets/returns a flag indicating whether the addresses of objects should be
  // written. The default is true.
  void EnableAddressPrinting(bool enable) {
    address_printing_enabled_ = enable;
  }
  bool IsAddressPrintingEnabled() const {
    return address_printing_enabled_;
  }

  // Prints the scene graph rooted by the given node.
  void PrintScene(const gfx::NodePtr& node, std::ostream& out);  // NOLINT

 private:
  Format format_;
  bool full_shape_printing_enabled_;
  bool address_printing_enabled_;
};

}  // namespace gfxutils
}  // namespace ion

#endif  // ION_GFXUTILS_PRINTER_H_
