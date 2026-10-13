#pragma once
// 粒子エフェクト（潮汐の飛沫・勝利の紙吹雪 — FR-014 / FR-015）

#include <random>
#include <vector>

namespace tidal::ui {

class Fx {
public:
    Fx();
    void update(float dt_sec);
    void draw() const;
    void clear() { particles_.clear(); }

    // (x,y) から (dir_x,dir_y) 方向へ散る泡しぶき
    void foam(float x, float y, float dir_x, float dir_y, int count);
    // 画面上部から降る紙吹雪
    void confetti(int count);

private:
    enum class Kind { Foam, Confetti };
    struct Particle {
        Kind kind;
        float x, y, vx, vy;
        float life, max_life;
        float size;
        float rot, vrot;
        unsigned color;
    };

    float rand(float lo, float hi);

    std::vector<Particle> particles_;
    std::mt19937 rng_;
};

}  // namespace tidal::ui
