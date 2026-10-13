#pragma once
// ゲーム内ヘルプ（ui-contract.md: Help content contract / FR-011）

#include "game/render.hpp"

namespace tidal::ui {

const Button& help_close_button();
void draw_help(int now_ms, int mouse_x, int mouse_y);

}  // namespace tidal::ui
