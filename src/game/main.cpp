// 潮汐オセロ (Tidal Reversi) — エントリポイントとメインループ

#include "game/dx.hpp"

#include "game/render.hpp"
#include "game/scenes.hpp"
#include "game/sound.hpp"

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
    using namespace tidal::ui;

    SetOutApplicationLogValidFlag(FALSE);
    ChangeWindowMode(TRUE);
    SetGraphMode(kScreenW, kScreenH, 32);
    SetWindowSizeChangeEnableFlag(FALSE);
    SetMainWindowText(L"潮汐オセロ");
    SetAlwaysRunFlag(TRUE);
    SetWaitVSyncFlag(FALSE);  // フレームレートは下の固定 60FPS 制御で決める
    if (DxLib_Init() == -1) return -1;
    SetDrawScreen(DX_SCREEN_BACK);

    init_fonts();
    sound::init();

    {
        App app;
        bool prev_click = false;
        bool prev_h = false;
        bool prev_esc = false;
        constexpr int kFrameMs = 1000 / 60;
        int next_frame = GetNowCount();

        while (ProcessMessage() == 0 && !app.quit()) {
            Input in;
            GetMousePoint(&in.mouse_x, &in.mouse_y);
            const bool click = (GetMouseInput() & MOUSE_INPUT_LEFT) != 0;
            const bool h = CheckHitKey(KEY_INPUT_H) != 0;
            const bool esc = CheckHitKey(KEY_INPUT_ESCAPE) != 0;
            in.clicked = click && !prev_click;
            in.key_help = h && !prev_h;
            in.key_back = esc && !prev_esc;
            prev_click = click;
            prev_h = h;
            prev_esc = esc;

            app.update(in, GetNowCount());
            ClearDrawScreen();
            app.draw();
            ScreenFlip();

            // 固定 60FPS（遅れた場合は追いつこうとせず基準をリセット）
            next_frame += kFrameMs;
            const int wait = next_frame - GetNowCount();
            if (wait > 0) WaitTimer(wait);
            else next_frame = GetNowCount();
        }
    }  // App のデストラクタで AI スレッドを停止・合流させてから DxLib を終了する

    DxLib_End();
    return 0;
}
