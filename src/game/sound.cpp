#include "game/sound.hpp"

#include "game/dx.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>

namespace tidal::ui::sound {

namespace {

constexpr double kRate = 44100.0;
constexpr double kTau = 6.283185307179586;

std::array<int, static_cast<size_t>(Se::Count)> g_handles{};

// t(秒) → -1..1 の波形関数から 16bit モノラルのサウンドハンドルを作る
int synth(double seconds, double gain, const std::function<double(double)>& wave) {
    const auto n = static_cast<LONGLONG>(seconds * kRate);
    const int soft = MakeSoftSound1Ch16Bit44KHz(n);
    if (soft == -1) return -1;
    for (LONGLONG i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / kRate;
        const double v = std::clamp(wave(t) * gain, -1.0, 1.0);
        const int s = static_cast<int>(v * 32767.0);
        WriteSoftSoundData(soft, i, s, s);
    }
    const int handle = LoadSoundMemFromSoftSound(soft);
    DeleteSoftSound(soft);
    return handle;
}

// 決定論的な疑似ノイズ
double noise(double t) {
    auto x = static_cast<std::uint32_t>(t * kRate) * 2654435761u;
    x ^= x >> 15;
    x *= 2246822519u;
    x ^= x >> 13;
    return static_cast<double>(x & 0xFFFF) / 32767.5 - 1.0;
}

double chord(double t, std::initializer_list<double> freqs, double stagger, double len) {
    double v = 0.0;
    int k = 0;
    for (double f : freqs) {
        const double start = stagger * k++;
        if (t < start) continue;
        const double u = t - start;
        const double env = std::exp(-u * 3.0 / len) * std::min(1.0, u * 200.0);
        v += env * (std::sin(kTau * f * u) + 0.3 * std::sin(kTau * 2 * f * u));
    }
    return v / static_cast<double>(freqs.size());
}

}  // namespace

void init() {
    auto set = [](Se se, int h) { g_handles[static_cast<size_t>(se)] = h; };
    // 着石: 短いクリック
    set(Se::Place, synth(0.06, 0.55, [](double t) {
            return std::exp(-t * 90.0) * (0.6 * std::sin(kTau * 1300 * t) + 0.4 * noise(t));
        }));
    // 反転: 上昇系の短い掃引
    set(Se::Flip, synth(0.16, 0.30, [](double t) {
            const double f = 500 + 2600 * t;
            return std::exp(-t * 18.0) * (std::sin(kTau * f * t) * 0.7 + noise(t) * 0.3);
        }));
    // 潮汐: 低い掃引音 + 波の擦れ
    set(Se::Tide, synth(0.9, 0.55, [](double t) {
            const double env = std::sin(3.14159265 * std::min(t / 0.9, 1.0));
            const double f = 180.0 - 110.0 * t;
            double lp = 0.0;
            for (int k = 0; k < 6; ++k) lp += noise(t - k / kRate);
            return env * (0.6 * std::sin(kTau * f * t) + 0.25 * lp / 6.0);
        }));
    // パス: 低い警告音 2 回
    set(Se::Pass, synth(0.32, 0.45, [](double t) {
            const double u = t < 0.16 ? t : t - 0.16;
            return std::exp(-u * 14.0) * std::sin(kTau * 220.0 * t) * std::min(1.0, u * 300.0);
        }));
    // 勝ち: 明るい長三和音のアルペジオ
    set(Se::Win, synth(1.4, 0.6, [](double t) { return chord(t, {523.25, 659.25, 783.99, 1046.5}, 0.08, 1.2); }));
    // 負け（対 CPU）: 短三和音
    set(Se::Lose, synth(1.2, 0.6, [](double t) { return chord(t, {440.0, 523.25, 659.25}, 0.12, 1.0); }));
    // 引き分け: 単音
    set(Se::Draw, synth(0.9, 0.6, [](double t) { return chord(t, {587.33}, 0.0, 0.8); }));
}

void play(Se se) {
    const int h = g_handles[static_cast<size_t>(se)];
    if (h > 0) PlaySoundMem(h, DX_PLAYTYPE_BACK);
}

}  // namespace tidal::ui::sound
