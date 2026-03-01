#include "../../../include/Simulation/Unit/UnitPhysicsSystem.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include "Core/Math.hpp"
#include <cmath>

namespace wr::simulation {

std::vector<entt::entity> UnitPhysicsSystem::getNearbyEntities(entt::registry& registry, entt::entity entity, const world::ChunkManager& chunkManager) {
    std::vector<entt::entity> nearby;
    nearby.reserve(128);

    auto& wp = registry.get<ecs::WorldPos>(entity);
    int cellTiles = core::CHUNK_SIZE / 16;
    int64_t centerCx = static_cast<int64_t>(std::floor(wp.wx / cellTiles));
    int64_t centerCy = static_cast<int64_t>(std::floor(wp.wy / cellTiles));

    const auto& chunks = chunkManager.getChunks();

    for (int64_t cy = centerCy - 1; cy <= centerCy + 1; ++cy) {
        for (int64_t cx = centerCx - 1; cx <= centerCx + 1; ++cx) {
            int64_t chunkX = static_cast<int64_t>(std::floor(static_cast<float>(cx) / 16.0f));
            int64_t chunkY = static_cast<int64_t>(std::floor(static_cast<float>(cy) / 16.0f));

            auto it = chunks.find({chunkX, chunkY});
            if (it != chunks.end()) {
                int localCx = cx - (chunkX * 16);
                int localCy = cy - (chunkY * 16);
                for (auto e : it->second->spatialCells[localCx][localCy]) {
                    if (registry.valid(e)) nearby.push_back(e);
                }
            }
        }
    }

    auto unitGroup = registry.view<ecs::UnitTag>();
    for (auto otherUnit : unitGroup) {
        if (otherUnit != entity) nearby.push_back(otherUnit);
    }

    return nearby;
}

void UnitPhysicsSystem::applyPhysicsAndCollisions(entt::registry& registry, entt::entity entity, float dt, world::ChunkManager& chunkManager, const std::vector<entt::entity>& nearbyEntities) {
    auto& vel = registry.get<ecs::Velocity>(entity);
    auto& wp = registry.get<ecs::WorldPos>(entity);
    auto& lp = registry.get<ecs::LogicalPos>(entity);
    auto& anim = registry.get<ecs::AnimationState>(entity);

    float colRad = 0.3f;
    float nextWx = wp.wx + vel.dx * dt;
    float nextWy = wp.wy + vel.dy * dt;

    int64_t currentX = static_cast<int64_t>(std::floor(wp.wx));
    int64_t currentY = static_cast<int64_t>(std::floor(wp.wy));

    uint8_t cL = chunkManager.getGlobalTileInfo(currentX, currentY).elevationLevel;

    auto isTileBlocked = [&](int64_t tx, int64_t ty) {
        return std::abs(chunkManager.getGlobalTileInfo(tx, ty).elevationLevel - cL) > 1;
    };

    auto isEntityBlocked = [&](float testX, float testY) -> bool {
        auto physicsGroup = registry.group<ecs::Hitbox>(entt::get<ecs::WorldPos>);
        for (auto solidEnt : nearbyEntities) {
            if (solidEnt == entity || !registry.all_of<ecs::SolidTag>(solidEnt) || !physicsGroup.contains(solidEnt)) continue;

            const auto& solidHb = physicsGroup.get<ecs::Hitbox>(solidEnt);
            const auto& solidWp = physicsGroup.get<ecs::WorldPos>(solidEnt);

            float minRadius = colRad + (solidHb.radius / 64.0f);
            float dx = std::abs(testX - solidWp.wx);
            float dy = std::abs(testY - solidWp.wy);

            if (dx <= minRadius && dy <= minRadius && (dx*dx + dy*dy < minRadius*minRadius)) return true;
        }
        return false;
    };

    if (vel.dx > 0.0f && isTileBlocked(std::floor(nextWx + colRad), currentY)) nextWx = static_cast<float>(currentX) + 1.0f - colRad - 0.001f;
    else if (vel.dx < 0.0f && isTileBlocked(std::floor(nextWx - colRad), currentY)) nextWx = static_cast<float>(currentX) + colRad + 0.001f;
    if (vel.dx != 0.0f && isEntityBlocked(nextWx, wp.wy)) nextWx = wp.wx;

    currentX = static_cast<int64_t>(std::floor(nextWx));

    if (vel.dy > 0.0f && isTileBlocked(currentX, std::floor(nextWy + colRad))) nextWy = static_cast<float>(currentY) + 1.0f - colRad - 0.001f;
    else if (vel.dy < 0.0f && isTileBlocked(currentX, std::floor(nextWy - colRad))) nextWy = static_cast<float>(currentY) + colRad + 0.001f;
    if (vel.dy != 0.0f && isEntityBlocked(nextWx, nextWy)) nextWy = wp.wy;

    wp.wx = nextWx;
    wp.wy = nextWy;
    lp.x = static_cast<int64_t>(std::floor(wp.wx));
    lp.y = static_cast<int64_t>(std::floor(wp.wy));

    auto newInfo = chunkManager.getGlobalTileInfo(lp.x, lp.y);
    wp.wasOnWater = wp.isOnWater;
    wp.isOnWater = (newInfo.type == core::TerrainType::Water);

    if (wp.isOnWater != wp.wasOnWater) {
        wp.zJumpVel = 100.0f;
        float speed = std::sqrt(vel.dx*vel.dx + vel.dy*vel.dy);
        if (speed > 0.01f && !anim.isActionLocked) {
            float dash = vel.baseSpeed * 0.5f;
            vel.dx = (vel.dx / speed) * dash;
            vel.dy = (vel.dy / speed) * dash;
        }
    }

    if (wp.zJumpVel > 0.0f || wp.zJump > 0.0f) {
        wp.zJump += wp.zJumpVel * dt;
        wp.zJumpVel -= 800.0f * dt;
        if (wp.zJump < 0.0f) { wp.zJump = 0.0f; wp.zJumpVel = 0.0f; }
    }

    if (wp.isOnWater) wp.targetZ = -16.0f;
    else if (newInfo.elevationLevel == 1) wp.targetZ = 0.0f;
    else if (newInfo.elevationLevel == 2) wp.targetZ = 32.0f;
    else if (newInfo.elevationLevel == 3) wp.targetZ = 64.0f;
    else if (newInfo.elevationLevel == 4) wp.targetZ = 96.0f;
    else if (newInfo.elevationLevel == 5) wp.targetZ = 128.0f;
    else if (newInfo.elevationLevel == 6) wp.targetZ = 160.0f;
    else if (newInfo.elevationLevel == 7) wp.targetZ = 192.0f;

    wp.wz = math::lerp(wp.wz, wp.targetZ, 12.0f * dt);
}

}