#include "Simulation/Unit/UnitControlSystem.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include <cmath>
#include <algorithm>

namespace wr::simulation {

void applyBuildingCollisions(entt::registry& registry, entt::entity entity, ecs::WorldPos& wp, const std::vector<entt::entity>& activeBuildings) {
    float uRad = 0.3f;
    if (auto* hb = registry.try_get<ecs::Hitbox>(entity)) uRad = hb->radius / 64.0f;
    else if (auto* ca = registry.try_get<ecs::ClickArea>(entity)) uRad = ca->radius / 64.0f;

    for (auto bEnt : activeBuildings) {
        auto& bWp = registry.get<ecs::WorldPos>(bEnt);

        float bRad = 0.4f;
        if (auto* bhb = registry.try_get<ecs::Hitbox>(bEnt)) bRad = bhb->radius / 64.0f;

        float minX = bWp.wx - bRad;
        float maxX = bWp.wx + bRad;
        float minY = bWp.wy - bRad;
        float maxY = bWp.wy + bRad;

        if (wp.wx > minX && wp.wx < maxX && wp.wy > minY && wp.wy < maxY) {

            float centerX = (minX + maxX) / 2.0f;
            float centerY = (minY + maxY) / 2.0f;

            float dirX = wp.wx - centerX;
            float dirY = wp.wy - centerY;
            float dist = std::sqrt(dirX * dirX + dirY * dirY);

            if (dist < 0.001f) {
                dirX = 1.0f;
                dirY = 0.0f;
                dist = 1.0f;
            }

            float shoveForce = 0.08f;
            wp.wx += (dirX / dist) * shoveForce;
            wp.wy += (dirY / dist) * shoveForce;

            continue;
        }

        float cx = std::clamp(wp.wx, minX, maxX);
        float cy = std::clamp(wp.wy, minY, maxY);

        float distSq = (wp.wx - cx)*(wp.wx - cx) + (wp.wy - cy)*(wp.wy - cy);
        if (distSq < uRad * uRad) {
            if (distSq == 0) {
                wp.wx += uRad;
                continue;
            }
            float dist = std::sqrt(distSq);
            float overlap = uRad - dist;
            wp.wx += ((wp.wx - cx) / dist) * overlap;
            wp.wy += ((wp.wy - cy) / dist) * overlap;
        }
    }
}

}