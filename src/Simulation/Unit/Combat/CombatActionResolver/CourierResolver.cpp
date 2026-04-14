#include "Simulation/Unit/Combat/CombatActionResolver.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include "Simulation/Environment/ResourceSystem.hpp"
#include "Rendering/TileHandler.hpp"
#include "Core/Types.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace wr::simulation {

void resolveCourierAction(entt::registry& registry, entt::entity entity, ecs::UnitData& uData, ecs::WorldPos& wp, ecs::AnimationState& anim, ecs::ActionTarget* actionTarget, world::ChunkManager& chunkManager, DeferredQueues& dq) {

    if (registry.all_of<ecs::TaskArea>(actionTarget->target) || registry.all_of<ecs::BuildingTag>(actionTarget->target)) {
        if (uData.heldItem != ecs::HeldItem::None) {
            auto itemToDrop = uData.heldItem;
            auto itemSubtype = uData.heldItemSubtype;

            uData.heldItem = ecs::HeldItem::None;
            uData.heldItemSubtype = 0;
            anim.isActionLocked = false;

            dq.removeActionTarget.push_back(entity);
            dq.removePath.push_back(entity);
            dq.removePathRequest.push_back(entity);

            dq.complex.push_back([&registry, target = actionTarget->target, itemToDrop, itemSubtype, z = wp.wz, wpx = wp.wx, wpy = wp.wy, chunkMgrPtr = &chunkManager]() {
                if (!registry.valid(target)) return;

                float minX, maxX, minY, maxY;
                bool isCityStorage = false;
                float shrink = 0.5f;

                if (registry.all_of<ecs::TaskArea>(target)) {
                    auto& task = registry.get<ecs::TaskArea>(target);
                    if (task.hasDropoff) {
                        minX = std::min(task.dropoffStart.x, task.dropoffEnd.x) + shrink;
                        maxX = std::max(task.dropoffStart.x, task.dropoffEnd.x) - shrink;
                        minY = std::min(task.dropoffStart.y, task.dropoffEnd.y) + shrink;
                        maxY = std::max(task.dropoffStart.y, task.dropoffEnd.y) - shrink;
                    } else {

                        auto sView = registry.view<ecs::CityStorageTag, ecs::WorldPos, ecs::ConstructionData>();
                        if (sView.begin() != sView.end()) {
                            auto sEnt = *sView.begin();
                            auto& sWp = sView.get<ecs::WorldPos>(sEnt);
                            auto& cData = sView.get<ecs::ConstructionData>(sEnt);
                            isCityStorage = true;
                            shrink = 0.4f;
                            minX = (sWp.wx - cData.resourceZoneWidth / 2.0f) + shrink;
                            maxX = (sWp.wx + cData.resourceZoneWidth / 2.0f) - shrink;
                            minY = (sWp.wy - cData.resourceZoneHeight / 2.0f) + shrink;
                            maxY = (sWp.wy + cData.resourceZoneHeight / 2.0f) - shrink;
                        } else {
                            minX = wpx - 0.5f; maxX = wpx + 0.5f;
                            minY = wpy - 0.5f; maxY = wpy + 0.5f;
                        }
                    }
                } else if (registry.all_of<ecs::BuildingTag>(target)) {
                    auto& cData = registry.get<ecs::ConstructionData>(target);
                    auto& bWp = registry.get<ecs::WorldPos>(target);
                    isCityStorage = registry.all_of<ecs::CityStorageTag>(target);
                    shrink = isCityStorage ? 0.4f : 0.5f;

                    minX = (bWp.wx - cData.resourceZoneWidth / 2.0f) + shrink;
                    maxX = (bWp.wx + cData.resourceZoneWidth / 2.0f) - shrink;
                    minY = (bWp.wy - cData.resourceZoneHeight / 2.0f) + shrink;
                    maxY = (bWp.wy + cData.resourceZoneHeight / 2.0f) - shrink;
                } else {
                    return;
                }

                if (maxX <= minX) { float mid = (minX + maxX)/2.f; minX = mid - 0.1f; maxX = mid + 0.1f; }
                if (maxY <= minY) { float mid = (minY + maxY)/2.f; minY = mid - 0.1f; maxY = mid + 0.1f; }

                int itemsInZone = 0;
                if (itemToDrop == ecs::HeldItem::Wood) {
                    auto logView = registry.view<ecs::LogTag, ecs::WorldPos>();
                    for (auto l : logView) {
                        auto& lWp = logView.get<ecs::WorldPos>(l);
                        if (lWp.wx >= minX && lWp.wx <= maxX && lWp.wy >= minY && lWp.wy <= maxY) itemsInZone++;
                    }
                } else if (itemToDrop == ecs::HeldItem::Rock) {
                    auto rockView = registry.view<ecs::SmallRockTag, ecs::WorldPos>();
                    for (auto r : rockView) {
                        auto& rWp = rockView.get<ecs::WorldPos>(r);
                        if (rWp.wx >= minX && rWp.wx <= maxX && rWp.wy >= minY && rWp.wy <= maxY) itemsInZone++;
                    }
                }

                float gridSpace = 0.5f;
                int cols = std::max(1, static_cast<int>((maxX - minX) / gridSpace));
                int rows = std::max(1, static_cast<int>((maxY - minY) / gridSpace));

                int layerCapacity = cols * rows;
                int indexInLayer = itemsInZone % layerCapacity;
                int currentLayer = itemsInZone / layerCapacity;

                int cellX = indexInLayer % cols;
                int cellY = indexInLayer / cols;

                float dropX = minX + cellX * gridSpace + gridSpace / 2.0f;
                float dropY = minY + cellY * gridSpace + gridSpace / 2.0f;
                float dropZ = z;

                if (currentLayer > 0) {
                    float scatterIntensity = isCityStorage ? 0.4f : 0.25f;
                    dropX += ((std::rand() % 100) / 100.0f - 0.5f) * scatterIntensity;
                    dropY += ((std::rand() % 100) / 100.0f - 0.5f) * scatterIntensity;
                    dropZ += currentLayer * 8.0f;
                }

                dropX = std::clamp(dropX, minX + 0.1f, maxX - 0.1f);
                dropY = std::clamp(dropY, minY + 0.1f, maxY - 0.1f);

                auto* tilesPtr = registry.ctx().find<const rendering::TileHandler*>();
                if (tilesPtr) {
                    if (itemToDrop == ecs::HeldItem::Wood) world::ResourceSystem::spawnLogs(registry, **tilesPtr, {dropX, dropY}, dropZ, 1, chunkMgrPtr, entt::null, true, itemSubtype);
                    else if (itemToDrop == ecs::HeldItem::Rock) world::ResourceSystem::spawnSmallRocks(registry, **tilesPtr, {dropX, dropY}, dropZ, 1, chunkMgrPtr, entt::null, true, itemSubtype);
                }
            });
        }
    }

    else {
        dq.complex.push_back([&registry, workerEnt = entity, target = actionTarget->target]() {
            if (!registry.valid(target) || !registry.valid(workerEnt)) return;

            auto& anim = registry.get<ecs::AnimationState>(workerEnt);
            auto& uData = registry.get<ecs::UnitData>(workerEnt);
            bool destroyedByCourier = false;

            if (registry.all_of<ecs::LogTag>(target)) {
                uData.heldItem = ecs::HeldItem::Wood;
                uData.heldItemSubtype = registry.get<ecs::ResourceData>(target).type;
                registry.destroy(target);
                destroyedByCourier = true;
            } else if (registry.all_of<ecs::SmallRockTag>(target)) {
                uData.heldItem = ecs::HeldItem::Rock;
                uData.heldItemSubtype = registry.get<ecs::ResourceData>(target).type;
                registry.destroy(target);
                destroyedByCourier = true;
            }

            if (destroyedByCourier) {
                anim.isActionLocked = false;
                anim.currentAnim = 0;
                anim.currentFrame = 0;
                anim.comboRequested = false;

                registry.remove<ecs::ActionTarget>(workerEnt);
                registry.remove<ecs::Path>(workerEnt);
                registry.remove<ecs::PathRequest>(workerEnt);
            }
        });
    }
}

}