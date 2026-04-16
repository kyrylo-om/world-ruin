#include "../../../../../include/Simulation/Unit/Combat/CombatActionResolver.hpp"
#include "ECS/Tags.hpp"
#include <cmath>

namespace wr::simulation {

void resolveCourierAction(entt::registry& registry, entt::entity entity, ecs::UnitData& uData, ecs::WorldPos& wp, ecs::AnimationState& anim, ecs::ActionTarget* actionTarget, world::ChunkManager& chunkManager, DeferredQueues& dq);
void resolveBuilderAction(entt::registry& registry, entt::entity entity, ecs::UnitData& uData, ecs::AnimationState& anim, ecs::ActionTarget* actionTarget, DeferredQueues& dq);
void resolveHarvesterAction(entt::registry& registry, entt::entity entity, ecs::UnitData& uData, ecs::AnimationState& anim, ecs::ActionTarget* actionTarget, DeferredQueues& dq);

void CombatActionResolver::resolve(entt::registry& registry, entt::entity entity, ecs::UnitData& uData, ecs::WorkerState& wState, ecs::CombatStats* cStats, ecs::WorldPos& wp, ecs::Velocity& vel, ecs::AnimationState& anim, ecs::ActionTarget* actionTarget, bool isHero, world::ChunkManager& chunkManager, DeferredQueues& dq) {

    if (anim.currentAnim == 2 || anim.currentAnim == 3) {
        if (anim.currentFrame == 0) anim.damageDealt = false;

        int damageFrame = (uData.type == ecs::UnitType::Warrior) ? 2 : 3;
        if (uData.type == ecs::UnitType::Courier) damageFrame = 1;
        if (uData.type == ecs::UnitType::Builder) damageFrame = 1;

        if (anim.currentFrame == damageFrame && !anim.damageDealt) {
            anim.damageDealt = true;

            entt::entity targetToHit = (actionTarget && registry.valid(actionTarget->target)) ? actionTarget->target : entt::null;

            if (!registry.valid(targetToHit) && isHero && uData.type != ecs::UnitType::Builder && uData.type != ecs::UnitType::Courier) {
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
                                if (dot >= maxCosAngle && distSq < bestDistSq) {
                                    bestDistSq = distSq;
                                    targetToHit = r;
                                }
                            } else {
                                bestDistSq = 0.0f;
                                targetToHit = r;
                            }
                        }
                    }
                }
            }

            if (registry.valid(targetToHit)) {

                ecs::ActionTarget hitTarget{targetToHit};

                if (uData.type == ecs::UnitType::Courier) {
                    resolveCourierAction(registry, entity, uData, wp, anim, &hitTarget, chunkManager, dq);
                }
                else if (uData.type == ecs::UnitType::Builder) {
                    resolveBuilderAction(registry, entity, uData, anim, &hitTarget, dq);
                }
                else {
                    resolveHarvesterAction(registry, entity, uData, anim, &hitTarget, dq);
                }
            } else {

                if (!isHero) {
                    anim.isActionLocked = false;
                    anim.currentAnim = 0;
                    anim.currentFrame = 0;
                    anim.comboRequested = false;
                    if (actionTarget) dq.removeActionTarget.push_back(entity);
                }
            }
        }
    }
}

}