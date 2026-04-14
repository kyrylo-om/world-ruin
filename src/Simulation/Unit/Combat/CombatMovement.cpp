#include "../../../../include/Simulation/Unit/Combat/CombatMovement.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace wr::simulation {

math::Vec2f CombatMovement::calculateIntent(entt::registry& registry, entt::entity entity, ecs::UnitData& uData, ecs::WorkerState& wState, ecs::CombatStats* cStats, ecs::WorldPos& wp, ecs::Velocity& vel, ecs::ActionTarget* actionTarget, bool& outIsAttackingCommand, bool isHero, bool isSpacePressed, bool spaceJustPressed, bool isPaused, float dt, world::ChunkManager& chunkManager, DeferredQueues& dq) {
    math::Vec2f moveIntent{0.0f, 0.0f};

    if (isHero && !isPaused) {
        if (uData.type == ecs::UnitType::Courier) {
            if (spaceJustPressed) outIsAttackingCommand = true;
        } else if (uData.type != ecs::UnitType::Builder) {
            if (isSpacePressed) outIsAttackingCommand = true;
        }

        if (outIsAttackingCommand && uData.type != ecs::UnitType::Builder && uData.type != ecs::UnitType::Courier) {
            if (!actionTarget || !registry.valid(actionTarget->target)) {
                entt::entity bestTarget = entt::null;
                float bestDistSq = 999999.0f;

                float facingRad = vel.facingAngle * 3.14159265f / 180.0f;
                math::Vec2f facingDir = { std::cos(facingRad), std::sin(facingRad) };

                float maxDist = cStats ? cStats->attackRadius : 0.85f;
                float maxCosAngle = std::cos((cStats ? cStats->arcDegrees : 135.0f) * 0.5f * 3.14159265f / 180.0f);

                auto resView = registry.view<ecs::WorldPos>();
                for (auto r : resView) {
                    bool isValid = false;
                    if (uData.type == ecs::UnitType::Warrior && registry.all_of<ecs::BushTag>(r)) isValid = true;
                    else if (uData.type == ecs::UnitType::Lumberjack && (registry.all_of<ecs::TreeTag>(r) || registry.all_of<ecs::BushTag>(r))) isValid = true;
                    else if (uData.type == ecs::UnitType::Miner && registry.all_of<ecs::RockTag>(r)) isValid = true;

                    if (isValid) {
                        auto& rWp = resView.get<ecs::WorldPos>(r);
                        float dx = rWp.wx - wp.wx;
                        float dy = rWp.wy - wp.wy;
                        float distSq = dx*dx + dy*dy;

                        float tRad = 0.0f;
                        if (auto* hb = registry.try_get<ecs::Hitbox>(r)) tRad = hb->radius / 64.0f;
                        else if (auto* ca = registry.try_get<ecs::ClickArea>(r)) tRad = ca->radius / 64.0f;

                        float effDist = maxDist + tRad + 0.5f;

                        if (distSq <= effDist * effDist) {
                            float dist = std::sqrt(distSq);
                            if (dist > 0.001f) {
                                float dot = (dx * facingDir.x + dy * facingDir.y) / dist;
                                if (dot >= maxCosAngle) {
                                    if (distSq < bestDistSq) {
                                        bestDistSq = distSq;
                                        bestTarget = r;
                                    }
                                }
                            } else {
                                bestDistSq = 0.0f;
                                bestTarget = r;
                            }
                        }
                    }
                }

                if (bestTarget != entt::null) {
                    dq.setActionTarget.push_back({entity, bestTarget});
                }
            }
        }
    }

    bool hasTarget = false;
    math::Vec2f targetPos;
    float targetRadBlocks = 0.0f;

    if (actionTarget && registry.valid(actionTarget->target)) {
        if (registry.all_of<ecs::Health>(actionTarget->target)) {
            auto* health = registry.try_get<ecs::Health>(actionTarget->target);
            if (health && health->current > 0) {
                hasTarget = true;
                auto& tWp = registry.get<ecs::WorldPos>(actionTarget->target);
                targetPos = {tWp.wx, tWp.wy};
                if (auto* hb = registry.try_get<ecs::Hitbox>(actionTarget->target)) targetRadBlocks = hb->radius / 64.0f;
                else if (auto* ca = registry.try_get<ecs::ClickArea>(actionTarget->target)) targetRadBlocks = ca->radius / 64.0f;
            } else {
                dq.removeActionTarget.push_back(entity);
                dq.removePath.push_back(entity);
                dq.removePathRequest.push_back(entity);
            }
        } else if (registry.all_of<ecs::TaskArea>(actionTarget->target)) {
            hasTarget = true;
            auto& task = registry.get<ecs::TaskArea>(actionTarget->target);
            if (task.hasDropoff) {
                targetPos = {(task.dropoffStart.x + task.dropoffEnd.x)/2.f, (task.dropoffStart.y + task.dropoffEnd.y)/2.f};
                targetRadBlocks = 1.0f;
            } else {
                auto storeView = registry.view<ecs::CityStorageTag, ecs::WorldPos, ecs::ConstructionData>();
                if (storeView.begin() != storeView.end()) {
                    auto sEnt = *storeView.begin();
                    auto& sWp = storeView.get<ecs::WorldPos>(sEnt);
                    auto& cData = storeView.get<ecs::ConstructionData>(sEnt);
                    targetPos = {sWp.wx, sWp.wy};
                    targetRadBlocks = (cData.resourceZoneWidth / 2.0f) - 0.5f;
                } else {
                    targetPos = {wp.wx, wp.wy};
                    targetRadBlocks = 1.0f;
                }
            }
        } else if (registry.all_of<ecs::BuildingTag>(actionTarget->target)) {
            hasTarget = true;
            auto& tWp = registry.get<ecs::WorldPos>(actionTarget->target);
            targetPos = {tWp.wx, tWp.wy};
            if (auto* hb = registry.try_get<ecs::Hitbox>(actionTarget->target)) targetRadBlocks = hb->radius / 64.0f;
            else if (auto* ca = registry.try_get<ecs::ClickArea>(actionTarget->target)) targetRadBlocks = ca->radius / 64.0f;
        } else {
            dq.removeActionTarget.push_back(entity);
            dq.removePath.push_back(entity);
            dq.removePathRequest.push_back(entity);
        }
    }

    if (hasTarget && !isPaused) {
        bool shouldMove = true;

        if (uData.type == ecs::UnitType::Courier && uData.heldItem != ecs::HeldItem::None && registry.all_of<ecs::BuildingTag>(actionTarget->target) && !registry.all_of<ecs::CityStorageTag>(actionTarget->target)) {
            auto& cData = registry.get<ecs::ConstructionData>(actionTarget->target);
            bool isFull = cData.isBuilt;

            if (!isFull) {
                float nextProgress = cData.buildProgress + 0.3f;
                float progressRatio = std::min(nextProgress / cData.maxTime, 1.0f);
                int expectedWood = static_cast<int>(cData.initialWood * progressRatio);
                int expectedRock = static_cast<int>(cData.initialRock * progressRatio);

                bool needsWood = (cData.initialWood - cData.woodRequired) < expectedWood;
                bool needsRock = (cData.initialRock - cData.rockRequired) < expectedRock;

                float minX = targetPos.x - cData.resourceZoneWidth / 2.0f;
                float maxX = targetPos.x + cData.resourceZoneWidth / 2.0f;
                float minY = targetPos.y - cData.resourceZoneHeight / 2.0f;
                float maxY = targetPos.y + cData.resourceZoneHeight / 2.0f;

                int woodOnGround = 0;
                int rockOnGround = 0;
                auto resView = registry.view<ecs::WorldPos, ecs::ResourceTag>();
                for (auto r : resView) {
                    auto& rWp = resView.get<ecs::WorldPos>(r);
                    if (rWp.wx >= minX && rWp.wx <= maxX && rWp.wy >= minY && rWp.wy <= maxY) {
                        if (registry.all_of<ecs::LogTag>(r)) woodOnGround++;
                        if (registry.all_of<ecs::SmallRockTag>(r)) rockOnGround++;
                    }
                }

                if (uData.heldItem == ecs::HeldItem::Wood && (woodOnGround >= cData.woodRequired || !needsWood)) isFull = true;
                if (uData.heldItem == ecs::HeldItem::Rock && (rockOnGround >= cData.rockRequired || !needsRock)) isFull = true;
            }

            if (isFull) {

                if (wState.currentTask != entt::null) {
                    dq.setActionTarget.push_back({entity, wState.currentTask});
                    dq.removePath.push_back(entity);
                    dq.removePathRequest.push_back(entity);
                    return {0.0f, 0.0f};
                }
            }
        }

        if (registry.all_of<ecs::BuildingTag>(actionTarget->target) && uData.type == ecs::UnitType::Builder) {
            auto& cData = registry.get<ecs::ConstructionData>(actionTarget->target);

            if (cData.isBuilt) {
                shouldMove = false;
                dq.removeActionTarget.push_back(entity);
                dq.removePath.push_back(entity);
                dq.removePathRequest.push_back(entity);
                wState.currentTask = entt::null;
            } else {
                float nextProgress = cData.buildProgress + 0.3f;
                float progressRatio = std::min(nextProgress / cData.maxTime, 1.0f);
                int expectedWood = static_cast<int>(cData.initialWood * progressRatio);
                int expectedRock = static_cast<int>(cData.initialRock * progressRatio);

                bool needsWood = (cData.initialWood - cData.woodRequired) < expectedWood;
                bool needsRock = (cData.initialRock - cData.rockRequired) < expectedRock;
                bool canConsume = true;

                if (needsWood || needsRock) {
                    float minX = targetPos.x - cData.resourceZoneWidth / 2.0f;
                    float maxX = targetPos.x + cData.resourceZoneWidth / 2.0f;
                    float minY = targetPos.y - cData.resourceZoneHeight / 2.0f;
                    float maxY = targetPos.y + cData.resourceZoneHeight / 2.0f;

                    int woodOnGround = 0;
                    int rockOnGround = 0;
                    auto resView = registry.view<ecs::WorldPos, ecs::ResourceTag>();
                    for (auto r : resView) {
                        auto& rWp = resView.get<ecs::WorldPos>(r);
                        if (rWp.wx >= minX && rWp.wx <= maxX && rWp.wy >= minY && rWp.wy <= maxY) {
                            if (registry.all_of<ecs::LogTag>(r)) woodOnGround++;
                            if (registry.all_of<ecs::SmallRockTag>(r)) rockOnGround++;
                        }
                    }

                    if (needsWood && woodOnGround == 0) canConsume = false;
                    if (needsRock && rockOnGround == 0) canConsume = false;
                }

                if (!canConsume) {
                    shouldMove = false;
                } else {
                    wState.workTimer -= dt;
                    if (wState.workTimer <= 0.0f) {
                        auto& bWp = registry.get<ecs::WorldPos>(actionTarget->target);
                        int baseBx = static_cast<int>(std::floor(bWp.wx));
                        int baseBy = static_cast<int>(std::floor(bWp.wy));
                        uint8_t baseLevel = chunkManager.getGlobalTileInfo(baseBx, baseBy).elevationLevel;

                        for (int attempt = 0; attempt < 15; ++attempt) {
                            float angle = (std::rand() % 360) * 3.14159f / 180.0f;
                            float r = targetRadBlocks + 0.8f;
                            math::Vec2f offset = {std::cos(angle) * r, std::sin(angle) * r};

                            int destBx = static_cast<int>(std::floor(bWp.wx + offset.x));
                            int destBy = static_cast<int>(std::floor(bWp.wy + offset.y));
                            auto info = chunkManager.getGlobalTileInfo(destBx, destBy);

                            if (info.elevationLevel == baseLevel && info.type != core::TerrainType::Water && !info.isRamp) {
                                wState.workOffset = offset;
                                break;
                            }
                        }
                        wState.workTimer = 3.0f + (std::rand() % 300) / 100.0f;
                        dq.removePath.push_back(entity);
                    }
                    targetPos.x += wState.workOffset.x;
                    targetPos.y += wState.workOffset.y;
                    targetRadBlocks = 0.2f;
                }
            }
        }

        if (shouldMove) {
            float dx = targetPos.x - wp.wx;
            float dy = targetPos.y - wp.wy;
            float dist = std::sqrt(dx * dx + dy * dy);
            float effectiveRadius = (cStats ? cStats->attackRadius : 0.68f) + targetRadBlocks;

            if (uData.type == ecs::UnitType::Courier && registry.all_of<ecs::BuildingTag>(actionTarget->target)) {
                effectiveRadius += 1.2f;
            }

            if (dist > effectiveRadius - 0.1f) {
                if (!registry.all_of<ecs::Path>(entity) && !registry.all_of<ecs::PathRequest>(entity)) {
                    moveIntent = {dx / dist, dy / dist};
                }
                vel.facingAngle = std::atan2(dy, dx) * 180.0f / 3.14159265f;
            } else {
                outIsAttackingCommand = true;
                dq.removePath.push_back(entity);

                if (uData.type == ecs::UnitType::Builder) {
                    auto& tWp = registry.get<ecs::WorldPos>(actionTarget->target);
                    float bDx = tWp.wx - wp.wx;
                    float bDy = tWp.wy - wp.wy;
                    vel.facingAngle = std::atan2(bDy, bDx) * 180.0f / 3.14159265f;
                } else {
                    vel.facingAngle = std::atan2(dy, dx) * 180.0f / 3.14159265f;
                }
            }
        }
    }
    return moveIntent;
}

}