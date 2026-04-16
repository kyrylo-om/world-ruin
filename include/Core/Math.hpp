#pragma once

#include "Types.hpp"
#include <cmath>
#include <functional>

namespace wr::math {

template <typename T>
[[nodiscard]] constexpr T lerp(T start, T end, float t) noexcept {
    return static_cast<T>(start + t * (end - start));
}

template<typename T>
struct Vec2 {
    T x{0};
    T y{0};

    [[nodiscard]] constexpr bool operator==(const Vec2& other) const noexcept {
        return x == other.x && y == other.y;
    }

    [[nodiscard]] constexpr bool operator!=(const Vec2& other) const noexcept {
        return !(*this == other);
    }

    constexpr Vec2& operator+=(const Vec2& other) noexcept {
        x += other.x; y += other.y; return *this;
    }

    [[nodiscard]] constexpr Vec2 operator+(const Vec2& other) const noexcept {
        return Vec2{x + other.x, y + other.y};
    }
};

using Vec2i64 = Vec2<core::GlobalCoord>;
using Vec2f   = Vec2<float>;

[[nodiscard]] inline constexpr Vec2i64 worldToChunk(core::GlobalCoord worldX, core::GlobalCoord worldY) noexcept {
    core::GlobalCoord cx = (worldX < 0) ? ((worldX + 1) / core::CHUNK_SIZE) - 1 : (worldX / core::CHUNK_SIZE);
    core::GlobalCoord cy = (worldY < 0) ? ((worldY + 1) / core::CHUNK_SIZE) - 1 : (worldY / core::CHUNK_SIZE);
    return {cx, cy};
}

struct SpatialHash {
    [[nodiscard]] std::size_t operator()(const Vec2i64& v) const noexcept {
        std::size_t h1 = std::hash<core::GlobalCoord>{}(v.x);
        std::size_t h2 = std::hash<core::GlobalCoord>{}(v.y);

        return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
};

}