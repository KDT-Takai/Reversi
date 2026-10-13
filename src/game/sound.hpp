#pragma once
// 合成 PCM 効果音（外部アセットなし — ui-contract.md: Sound contract）

namespace tidal::ui::sound {

enum class Se { Place, Flip, Tide, Pass, Win, Lose, Draw, Count };

void init();
void play(Se se);

}  // namespace tidal::ui::sound
