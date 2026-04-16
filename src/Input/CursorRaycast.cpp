#include "Input/CursorRaycast.hpp"
#include <SFML/Window/Mouse.hpp>
#include <cmath>

namespace wr::input {

math::Vec2f CursorRaycast::inverseRotateCoordinate(const math::Vec2f& relativePos, float angleDegrees) noexcept {
    float rad = -angleDegrees * 3.14159265f / 180.0f;
    float cosA = std::cos(rad);
    float sinA = std::sin(rad);

    return {
        relativePos.x * cosA - relativePos.y * sinA,
        relativePos.x * sinA + relativePos.y * cosA
    };
}

math::Vec2f CursorRaycast::getHoveredWorldPos(const sf::RenderWindow& window, const rendering::Camera& camera, world::ChunkManager& chunkManager) noexcept {
    sf::Vector2i pixelPos = sf::Mouse::getPosition(window);
    sf::Vector2f viewPos = window.mapPixelToCoords(pixelPos, camera.getView());
    math::Vec2f relativePos{viewPos.x, viewPos.y};

    float angle = camera.getRotation();
    math::Vec2i64 camAbs = camera.getAbsolutePosition();
    math::Vec2f camSub = { camera.getSubPixelX(), camera.getSubPixelY() };

    float testZLevels[] = { 192.0f, 160.0f, 128.0f, 96.0f, 64.0f, 32.0f, 0.0f };

    for (float testZ : testZLevels) {

        math::Vec2f adjustedScreen = { relativePos.x, relativePos.y + testZ };
        math::Vec2f unrotatedPos = inverseRotateCoordinate(adjustedScreen, angle);

        float exactX = unrotatedPos.x + camSub.x;
        float exactY = unrotatedPos.y + camSub.y;

        math::Vec2f wPos;
        wPos.x = static_cast<float>(camAbs.x) + (exactX / 64.0f);
        wPos.y = static_cast<float>(camAbs.y) + (exactY / 64.0f);

        int64_t bx = static_cast<int64_t>(std::floor(wPos.x));
        int64_t by = static_cast<int64_t>(std::floor(wPos.y));

        auto info = chunkManager.getGlobalTileInfo(bx, by);
        float tileZ = 0.0f;
        if (info.elevationLevel == 2) tileZ = 32.0f;
        else if (info.elevationLevel == 3) tileZ = 64.0f;
        else if (info.elevationLevel == 4) tileZ = 96.0f;
        else if (info.elevationLevel == 5) tileZ = 128.0f;
        else if (info.elevationLevel == 6) tileZ = 160.0f;
        else if (info.elevationLevel == 7) tileZ = 192.0f;

        if (tileZ >= testZ) {
            return wPos;
        }
    }

    math::Vec2f unrotatedPos = inverseRotateCoordinate(relativePos, angle);
    math::Vec2f fallback;
    fallback.x = static_cast<float>(camAbs.x) + ((unrotatedPos.x + camSub.x) / 64.0f);
    fallback.y = static_cast<float>(camAbs.y) + ((unrotatedPos.y + camSub.y) / 64.0f);
    return fallback;
}

}