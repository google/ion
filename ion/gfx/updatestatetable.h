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

#ifndef ION_GFX_UPDATESTATETABLE_H_
#define ION_GFX_UPDATESTATETABLE_H_

namespace ion {
namespace gfx {

class GraphicsManager;
class StateTable;

// This internal function can be used to update a StateTable instance to match
// the current OpenGL settings as provided by a GraphicsManager instance. The
// default width and height are passed in to allow viewport and scissor box
// values to be set correctly.
void UpdateStateTable(int default_width, int default_height,
                      GraphicsManager* gm, StateTable* st);

// This internal function can be used to update a StateTable instance to match
// the current OpenGL settings as provided by a GraphicsManager instance. In
// contrast to UpdateStateTable(), above, only the values that are already set
// in the StateTable are updated.
void UpdateSettingsInStateTable(StateTable* st, GraphicsManager* gm);

// This internal function can be used to update the Clear()-related OpenGL state
// (dithering, scissor test, write masks, scissor box, and clear values) managed
// by a GraphicsManager to match a StateTable, and updates save_state to contain
// the new state. It calls the GraphicsManager to apply changes for all
// Clear()-related differences between new_state and save_state.
void ClearFromStateTable(const StateTable& new_state,
                         StateTable* save_state,
                         GraphicsManager* gm);

// This internal function can be used to update the OpenGL state managed by a
// GraphicsManager to match a StateTable, with respect to another StateTable
// that contains the current state. It calls the GraphicsManager to apply
// changes for all differences between old_state and new_state. Note that
// changes in clear state (write masks and scissor box) will be saved to
// save_state, but clear color, depth and stencil values are not updated or
// checked at all; use ClearFromStateTable() instead.
void UpdateFromStateTable(const StateTable& new_state,
                          StateTable* save_state,
                          GraphicsManager* gm);

}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_UPDATESTATETABLE_H_
