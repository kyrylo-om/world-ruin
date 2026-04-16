#include "../../../include/Simulation/Environment/ResourceUpdateSystem.hpp"
#include "../../../include/Simulation/Environment/ResourceSystem.hpp"
#include "Rendering/TileHandler.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include <cmath>
#include <vector>
#include <algorithm>

namespace wr::simulation {

void ResourceUpdateSystem::update(entt::registry& registry, float dt, const std::vector<world::Chunk*>& activeChunks, world::ChunkManager& chunkManager) {
    const auto* tilesPtr = registry.ctx().find<const rendering::TileHandler*>();

    auto updateDrop = [&](entt::entity entity, ecs::WorldPos& wp, ecs::Velocity& vel) -> bool {
        bool isMoving = (wp.zJump > 0.0f || wp.zJumpVel != 0.0f || std::abs(vel.dx) > 0.001f || std::abs(vel.dy) > 0.001f);
        if (!isMoving) {
            return false;
        }

        if (wp.zJump > 0.0f || wp.zJumpVel != 0.0f) {
            wp.wx += vel.dx * dt;
            wp.wy += vel.dy * dt;

            wp.zJumpVel -= 1200.0f * dt;
            wp.zJump += wp.zJumpVel * dt;

            vel.dx *= (1.0f - 2.0f * dt);
            vel.dy *= (1.0f - 2.0f * dt);

            if (wp.zJump <= 0.0f) {
                wp.zJump = 0.0f;

                if (wp.zJumpVel < -120.0f) {
                    wp.zJumpVel *= -0.3f;
                    vel.dx *= 0.7f;
                    vel.dy *= 0.7f;
                } else {
                    wp.zJumpVel = 0.0f;
                    vel.dx = 0.0f;
                    vel.dy = 0.0f;

                    if (auto* lp = registry.try_get<ecs::LogicalPos>(entity)) {
                        lp->x = static_cast<int64_t>(std::floor(wp.wx));
                        lp->y = static_cast<int64_t>(std::floor(wp.wy));
                    }
                }
            }
        }

        return (wp.zJump > 0.0f || wp.zJumpVel != 0.0f || std::abs(vel.dx) > 0.001f || std::abs(vel.dy) > 0.001f);
    };

    std::vector<entt::entity> brokenResources;
    std::vector<entt::entity> inactiveResources;
    auto activeResources = registry.view<ecs::ResourceData, ecs::ActiveResourceUpdateTag>();

    for (auto entity : activeResources) {
        auto& resData = activeResources.get<ecs::ResourceData>(entity);
        bool keepActive = false;

        if (auto* wp = registry.try_get<ecs::WorldPos>(entity)) {
            if (auto* vel = registry.try_get<ecs::Velocity>(entity)) {
                if (registry.all_of<ecs::LogTag>(entity) || registry.all_of<ecs::SmallRockTag>(entity)) {
                    if (updateDrop(entity, *wp, *vel)) keepActive = true;
                }
            }
        }

        if (resData.shakeTimer > 0.0f) {
            resData.shakeTimer -= dt;
            if (resData.shakeTimer < 0.0f) resData.shakeTimer = 0.0f;
            if (resData.shakeTimer > 0.0f) keepActive = true;
        }

        if (!resData.isDestroyed) {
            if (auto* health = registry.try_get<ecs::Health>(entity)) {
                if (health->current <= 0) {
                    resData.isDestroyed = true;
                    keepActive = true;
                    brokenResources.push_back(entity);

                    if (tilesPtr) {
                        auto marked = registry.try_get<ecs::MarkedForHarvestTag>(entity);
                        entt::entity taskEnt = marked ? marked->taskEntity : entt::null;
                        world::ResourceSystem::onResourceDestroyed(registry, entity, **tilesPtr, &chunkManager, taskEnt);
                    }
                }
            }
        }

        if (!keepActive && registry.valid(entity)) {
            inactiveResources.push_back(entity);
        }
    }

    for (auto e : brokenResources) registry.destroy(e);

    for (auto e : inactiveResources) {
        if (registry.valid(e) && registry.all_of<ecs::ActiveResourceUpdateTag>(e)) {
            registry.remove<ecs::ActiveResourceUpdateTag>(e);
        }
    }

    static float cleanupAccumulator = 0.0f;
    cleanupAccumulator += dt;
    const bool shouldCleanup = !brokenResources.empty() || cleanupAccumulator >= 0.25f;

    if (!shouldCleanup) {
        return;
    }

    cleanupAccumulator = 0.0f;

    if (activeChunks.empty()) {
        return;
    }

    static size_t cleanupCursor = 0;
    constexpr size_t CHUNKS_PER_CLEANUP_STEP = 4;

    cleanupCursor %= activeChunks.size();
    const size_t chunksToClean = std::min(CHUNKS_PER_CLEANUP_STEP, activeChunks.size());

    for (size_t i = 0; i < chunksToClean; ++i) {
        world::Chunk* chunk = activeChunks[(cleanupCursor + i) % activeChunks.size()];
        chunk->entities.erase(
            std::remove_if(chunk->entities.begin(), chunk->entities.end(),
                [&](entt::entity e) { return !registry.valid(e); }),
            chunk->entities.end()
        );

        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 16; ++x) {
                chunk->spatialCells[x][y].erase(
                    std::remove_if(chunk->spatialCells[x][y].begin(), chunk->spatialCells[x][y].end(),
                        [&](entt::entity e) { return !registry.valid(e); }),
                    chunk->spatialCells[x][y].end()
                );
            }
        }
    }

    cleanupCursor = (cleanupCursor + chunksToClean) % activeChunks.size();
}

}