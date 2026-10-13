#include "game/fx.hpp"

#include "game/dx.hpp"

#include <algorithm>
#include <cmath>

#include "game/render.hpp"

namespace tidal::ui {

namespace {
constexpr size_t kMaxParticles = 1500;
}

Fx::Fx() : rng_(std::random_device{}()) {}

float Fx::rand(float lo, float hi) { return std::uniform_real_distribution<float>(lo, hi)(rng_); }

void Fx::foam(float x, float y, float dir_x, float dir_y, int count) {
    for (int i = 0; i < count && particles_.size() < kMaxParticles; ++i) {
        const float speed = rand(60.0f, 260.0f);
        const float spread = rand(-0.9f, 0.9f);
        const float vx = dir_x * std::cos(spread) - dir_y * std::sin(spread);
        const float vy = dir_x * std::sin(spread) + dir_y * std::cos(spread);
        const int w = static_cast<int>(rand(180.0f, 255.0f));
        particles_.push_back({Kind::Foam, x + rand(-8, 8), y + rand(-8, 8), vx * speed, vy * speed, 0.0f,
                              rand(0.35f, 0.8f), rand(2.0f, 5.5f), 0.0f, 0.0f, GetColor(w, 255, 255)});
    }
}

void Fx::confetti(int count) {
    static const int kColors[][3] = {{255, 90, 90}, {255, 200, 60}, {90, 220, 120},
                                     {70, 180, 255}, {200, 120, 255}, {255, 255, 255}};
    for (int i = 0; i < count && particles_.size() < kMaxParticles; ++i) {
        const auto& c = kColors[std::uniform_int_distribution<int>(0, 5)(rng_)];
        particles_.push_back({Kind::Confetti, rand(0.0f, static_cast<float>(kScreenW)), rand(-120.0f, -10.0f),
                              rand(-60.0f, 60.0f), rand(80.0f, 220.0f), 0.0f, rand(3.0f, 5.0f), rand(5.0f, 9.0f),
                              rand(0.0f, 6.28f), rand(-9.0f, 9.0f), GetColor(c[0], c[1], c[2])});
    }
}

void Fx::update(float dt) {
    for (auto& p : particles_) {
        p.life += dt;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.rot += p.vrot * dt;
        if (p.kind == Kind::Foam) {
            p.vx *= 1.0f - 2.5f * dt;
            p.vy = p.vy * (1.0f - 2.5f * dt) + 140.0f * dt;
        } else {
            p.vx += std::sin(p.rot) * 40.0f * dt;  // ひらひら揺れる
            p.vy = std::min(p.vy + 60.0f * dt, 240.0f);
        }
    }
    std::erase_if(particles_, [](const Particle& p) {
        return p.life >= p.max_life || p.y > kScreenH + 40;
    });
}

void Fx::draw() const {
    for (const auto& p : particles_) {
        const float fade = 1.0f - p.life / p.max_life;
        if (p.kind == Kind::Foam) {
            SetDrawBlendMode(DX_BLENDMODE_ADD, static_cast<int>(220 * fade));
            DrawCircleAA(p.x, p.y, p.size * (0.6f + 0.4f * fade), 12, p.color, TRUE);
        } else {
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(255 * std::min(1.0f, fade * 2.0f)));
            DrawOvalAA(p.x, p.y, std::max(1.0f, p.size * std::abs(std::cos(p.rot))), p.size * 0.55f, 10, p.color,
                       TRUE);
        }
    }
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}

}  // namespace tidal::ui
