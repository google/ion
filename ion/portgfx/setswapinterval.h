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

#ifndef ION_PORTGFX_SETSWAPINTERVAL_H_
#define ION_PORTGFX_SETSWAPINTERVAL_H_

namespace ion {
namespace portgfx {

// Sets the swap interval of the current GL context on the default display
// device. Note that this function does nothing on iOS, Android, and Asmjs.
// The function returns whether the swap interval was successfully set, and
// always returns true on platforms where it does nothing.
ION_API bool SetSwapInterval(int interval);

}  // namespace portgfx
}  // namespace ion

#endif  // ION_PORTGFX_SETSWAPINTERVAL_H_
