#include "../../../include/Simulation/Unit/UnitPhysicsSystem.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include "Core/Math.hpp"
#include <cmath>
#include <chrono>

namespace wr::simulation {

void UnitPhysicsSystem::applyPhysicsAndCollisions(entt::registry& registry, entt::entity entity, ecs::WorldPos& wp, ecs::Velocity& vel, ecs::LogicalPos& lp, ecs::AnimationState& anim, float dt, const FastChunkCache& chunkCache, const EntitySpatialGrid& solidGrid, const EntitySpatialGrid& unitGrid, PhysicsTimers& pt) {
    auto t0 = std::chrono::high_resolution_clock::now();

    bool firstFrameSnap = (wp.targetZ <= -9000.0f);
    bool isActivelyMoving = (std::abs(vel.dx) > 0.001f || std::abs(vel.dy) > 0.001f || wp.zJumpVel != 0.0f || wp.zJump > 0.0f);

    float colRad = 0.3f;
    float nextWx = wp.wx + vel.dx * dt;
    float nextWy = wp.wy + vel.dy * dt;

    if (isActivelyMoving) {
        float sepX = 0.0f;
        float sepY = 0.0f;
        unitGrid.forEachNearby({wp.wx, wp.wy}, 0.7f, [&](const SpatialGridData& data) {
            if (data.entity == entity) return;
            float dx = wp.wx - data.x;
            float dy = wp.wy - data.y;
            float distSq = dx*dx + dy*dy;
            if (distSq > 0.0001f && distSq < 0.49f) {
                float dist = std::sqrt(distSq);
                float push = (0.7f - dist);
                sepX += (dx / dist) * push;
                sepY += (dy / dist) * push;
            }
        });

        float sepMag = std::sqrt(sepX*sepX + sepY*sepY);
        if (sepMag > 2.0f) {
            sepX = (sepX / sepMag) * 2.0f;
            sepY = (sepY / sepMag) * 2.0f;
        }

        nextWx += sepX * dt * 3.0f;
        nextWy += sepY * dt * 3.0f;
    }

    int64_t currentX = static_cast<int64_t>(std::floor(wp.wx));
    int64_t currentY = static_cast<int64_t>(std::floor(wp.wy));

    auto getTileData = [&](int64_t tx, int64_t ty) -> std::pair<uint8_t, core::TerrainType> {
        int64_t cx = (tx < 0) ? ((tx + 1) / 64) - 1 : (tx / 64);
        int64_t cy = (ty < 0) ? ((ty + 1) / 64) - 1 : (ty / 64);
        const auto* chunk = chunkCache.get(cx, cy);
        if (chunk) {
            int64_t lx = tx - cx * 64;
            int64_t ly = ty - cy * 64;
            if (lx >= 0 && lx < 64 && ly >= 0 && ly < 64) {
                return {chunk->getLevel(lx, ly), chunk->getTile(lx, ly)};
            }
        }
        return {1, core::TerrainType::Ground};
    };

    uint8_t cL = getTileData(currentX, currentY).first;

    auto isTileBlocked = [&](int64_t tx, int64_t ty) {
        return std::abs(getTileData(tx, ty).first - cL) > 1;
    };

    if (isActivelyMoving) {
        struct NeighborData { float x, y, r; };
        thread_local std::vector<NeighborData> solidNeighbors;
        solidNeighbors.clear();

        solidGrid.forEachNearby({wp.wx, wp.wy}, 2.0f, [&](const SpatialGridData& data) {
            if (data.entity == entity) return;
            solidNeighbors.push_back({data.x, data.y, data.radius});
        });

        auto t1 = std::chrono::high_resolution_clock::now();
        pt.gridQuery += std::chrono::duration<double, std::milli>(t1 - t0).count();

        auto isEntityBlocked = [&](float testX, float testY) -> bool {
            for (const auto& neighbor : solidNeighbors) {
                float minRadius = colRad + neighbor.r;
                float dx = std::abs(testX - neighbor.x);
                float dy = std::abs(testY - neighbor.y);

                if (dx <= minRadius && dy <= minRadius && (dx*dx + dy*dy < minRadius*minRadius)) return true;
            }
            return false;
        };

        if (nextWx - wp.wx > 0.0f && isTileBlocked(std::floor(nextWx + colRad), currentY)) nextWx = static_cast<float>(currentX) + 1.0f - colRad - 0.001f;
        else if (nextWx - wp.wx < 0.0f && isTileBlocked(std::floor(nextWx - colRad), currentY)) nextWx = static_cast<float>(currentX) + colRad + 0.001f;
        if (nextWx != wp.wx && isEntityBlocked(nextWx, wp.wy)) nextWx = wp.wx;

        currentX = static_cast<int64_t>(std::floor(nextWx));

        if (nextWy - wp.wy > 0.0f && isTileBlocked(currentX, std::floor(nextWy + colRad))) nextWy = static_cast<float>(currentY) + 1.0f - colRad - 0.001f;
        else if (nextWy - wp.wy < 0.0f && isTileBlocked(currentX, std::floor(nextWy - colRad))) nextWy = static_cast<float>(currentY) + colRad + 0.001f;
        if (nextWy != wp.wy && isEntityBlocked(nextWx, nextWy)) nextWy = wp.wy;

        pt.collisionMath += std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - t1).count();
    } else {
        pt.gridQuery += 0.0;
        pt.collisionMath += std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - t0).count();
    }

    auto t2 = std::chrono::high_resolution_clock::now();

    wp.wx = nextWx;
    wp.wy = nextWy;
    lp.x = static_cast<int64_t>(std::floor(wp.wx));
    lp.y = static_cast<int64_t>(std::floor(wp.wy));

    int64_t cx = (lp.x < 0) ? ((lp.x + 1) / 64) - 1 : (lp.x / 64);
    int64_t cy = (lp.y < 0) ? ((lp.y + 1) / 64) - 1 : (lp.y / 64);
    const auto* standingChunk = chunkCache.get(cx, cy);

    if (standingChunk) {
        int64_t lx = lp.x - cx * 64;
        int64_t ly = lp.y - cy * 64;
        uint8_t lvl = standingChunk->getLevel(lx, ly);
        core::TerrainType tType = standingChunk->getTile(lx, ly);

        wp.wasOnWater = wp.isOnWater;
        wp.isOnWater = (tType == core::TerrainType::Water);

        if (wp.isOnWater != wp.wasOnWater) {
            wp.zJumpVel = 100.0f;
            float speed = std::sqrt(vel.dx*vel.dx + vel.dy*vel.dy);
            if (speed > 0.01f && !anim.isActionLocked) {
                float dash = vel.baseSpeed * 0.5f;
                vel.dx = (vel.dx / speed) * dash;
                vel.dy = (vel.dy / speed) * dash;
            }
        }

        if (wp.isOnWater) wp.targetZ = -16.0f;
        else if (lvl == 1) wp.targetZ = 0.0f;
        else if (lvl == 2) wp.targetZ = 32.0f;
        else if (lvl == 3) wp.targetZ = 64.0f;
        else if (lvl == 4) wp.targetZ = 96.0f;
        else if (lvl == 5) wp.targetZ = 128.0f;
        else if (lvl == 6) wp.targetZ = 160.0f;
        else if (lvl == 7) wp.targetZ = 192.0f;

        if (firstFrameSnap) {
            wp.wz = wp.targetZ;
        } else {
            wp.wz = math::lerp(wp.wz, wp.targetZ, 12.0f * dt);
        }
    } else {
        wp.targetZ = -9999.0f;
    }

    if (wp.zJumpVel > 0.0f || wp.zJump > 0.0f) {
        wp.zJump += wp.zJumpVel * dt;
        wp.zJumpVel -= 800.0f * dt;
        if (wp.zJump < 0.0f) { wp.zJump = 0.0f; wp.zJumpVel = 0.0f; }
    }

    auto t3 = std::chrono::high_resolution_clock::now();
    pt.stateUpdate += std::chrono::duration<double, std::milli>(t3 - t2).count();
}

}