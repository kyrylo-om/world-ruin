#include "../../../../include/Rendering/Renderers/Overlay/OverlayUtils.hpp"
#include <cmath>

namespace wr::rendering::overlay {

    std::pair<uint8_t, float> getElevationInfo(const std::unordered_map<math::Vec2i64, std::unique_ptr<world::Chunk>, math::SpatialHash>& chunks, math::Vec2f start, math::Vec2f end) {
        float mx = (start.x + end.x) / 2.0f;
        float my = (start.y + end.y) / 2.0f;

        int64_t cx = static_cast<int64_t>(std::floor(mx / 64.0f));
        int64_t cy = static_cast<int64_t>(std::floor(my / 64.0f));

        uint8_t lvl = 1;
        auto it = chunks.find({cx, cy});
        if (it != chunks.end() && it->second->state.load(std::memory_order_acquire) == world::ChunkState::Active) {
            int64_t lx = static_cast<int64_t>(std::floor(mx)) - cx * 64;
            int64_t ly = static_cast<int64_t>(std::floor(my)) - cy * 64;
            if (lx >= 0 && lx < 64 && ly >= 0 && ly < 64) {
                lvl = it->second->getLevel(lx, ly);
            }
        }

        float z = 0.0f;
        if (lvl == 2) z = 32.0f;
        else if (lvl == 3) z = 64.0f;
        else if (lvl == 4) z = 96.0f;
        else if (lvl == 5) z = 128.0f;
        else if (lvl == 6) z = 160.0f;
        else if (lvl == 7) z = 192.0f;

        return {lvl, z};
    }

    sf::Vector2f toScreen(const RenderContext& ctx, math::Vec2f w, float zHeight) {
        float dx = (w.x - static_cast<float>(ctx.camera.getAbsolutePosition().x)) * 64.0f;
        float dy = (w.y - static_cast<float>(ctx.camera.getAbsolutePosition().y)) * 64.0f;
        math::Vec2f rel = { dx - ctx.camera.getSubPixelX(), dy - ctx.camera.getSubPixelY() };

        float rad = ctx.currentAngle * 3.14159265f / 180.0f;
        float rotX = rel.x * std::cos(rad) - rel.y * std::sin(rad);
        float rotY = rel.x * std::sin(rad) + rel.y * std::cos(rad);

        return sf::Vector2f(rotX, rotY - zHeight);
    }

}