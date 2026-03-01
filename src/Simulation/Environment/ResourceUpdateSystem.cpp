#include "../../../include/Simulation/Environment/ResourceUpdateSystem.hpp"
#include "../../../include/Simulation/Environment/ResourceSystem.hpp"
#include "Rendering/TileHandler.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include <cmath>

namespace wr::simulation {

void ResourceUpdateSystem::update(entt::registry& registry, float dt, const std::vector<world::Chunk*>& activeChunks, world::ChunkManager& chunkManager) {
    const auto* tilesPtr = registry.ctx().find<const rendering::TileHandler*>();

    auto dropView = registry.view<ecs::WorldPos, ecs::Velocity>();
    for (auto e : dropView) {
        if (!registry.all_of<ecs::LogTag>(e) && !registry.all_of<ecs::SmallRockTag>(e)) continue;

        auto& wp = dropView.get<ecs::WorldPos>(e);
        auto& vel = dropView.get<ecs::Velocity>(e);

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

                    if (auto* lp = registry.try_get<ecs::LogicalPos>(e)) {
                        lp->x = static_cast<int64_t>(std::floor(wp.wx));
                        lp->y = static_cast<int64_t>(std::floor(wp.wy));
                    }
                }
            }
        }
    }

    std::vector<entt::entity> brokenResources;

    for (world::Chunk* chunk : activeChunks) {
        for (entt::entity entity : chunk->entities) {
            if (!registry.valid(entity)) continue;

            if (auto* resData = registry.try_get<ecs::ResourceData>(entity)) {
                if (resData->shakeTimer > 0.0f) {
                    resData->shakeTimer -= dt;
                    if (resData->shakeTimer < 0.0f) resData->shakeTimer = 0.0f;
                }

                if (!resData->isDestroyed) {
                    if (auto* health = registry.try_get<ecs::Health>(entity)) {
                        if (health->current <= 0) {
                            resData->isDestroyed = true;
                            brokenResources.push_back(entity);

                            if (tilesPtr) {
                                auto marked = registry.try_get<ecs::MarkedForHarvestTag>(entity);
                                entt::entity taskEnt = marked ? marked->taskEntity : entt::null;
                                world::ResourceSystem::onResourceDestroyed(registry, entity, **tilesPtr, &chunkManager, taskEnt);
                            }
                        }
                    }
                }
            }
        }
    }

    for (auto e : brokenResources) registry.destroy(e);

    for (world::Chunk* chunk : activeChunks) {
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
}

}