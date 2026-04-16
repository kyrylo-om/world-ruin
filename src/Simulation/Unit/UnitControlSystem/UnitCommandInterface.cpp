#include "Simulation/Unit/UnitControlSystem.hpp"
#include "Simulation/Unit/UnitTaskSystem.hpp"
#include "Simulation/Environment/ResourceSystem.hpp"
#include "Rendering/TileHandler.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"

namespace wr::simulation {

void processGlobalCommands(entt::registry& registry, bool pJustPressed, bool isCancelCommand) {
    if (pJustPressed) {
        auto selectedView = registry.view<ecs::SelectedTag>();
        for (auto entity : selectedView) {
            if (registry.all_of<ecs::PausedTag>(entity)) {
                registry.remove<ecs::PausedTag>(entity);
                registry.remove<ecs::IdleTag>(entity);
            } else {
                registry.emplace<ecs::PausedTag>(entity);
                registry.remove<ecs::Path>(entity);
                registry.remove<ecs::PathRequest>(entity);
            }
        }
    }

    if (isCancelCommand) {
        UnitTaskSystem::cancelSelectedTasks(registry);
    }
}

void handleHeroInput(entt::registry& registry, entt::entity entity, ecs::UnitData& uData, ecs::WorkerState& wState, ecs::WorldPos& wp, ecs::AnimationState& anim, bool passSpaceJustPressed, bool isTaskMode, bool isBuilderMode, world::ChunkManager& chunkManager, DeferredQueues& localQueues) {
    if (uData.type == ecs::UnitType::Courier && passSpaceJustPressed && uData.heldItem != ecs::HeldItem::None && !isTaskMode && !isBuilderMode) {
        auto itemToDrop = uData.heldItem;
        auto itemSubtype = uData.heldItemSubtype;
        uData.heldItem = ecs::HeldItem::None;
        uData.heldItemSubtype = 0;

        localQueues.complex.push_back([&registry, itemToDrop, itemSubtype, wpos = wp, tilesPtr = registry.ctx().find<const rendering::TileHandler*>(), chunkMgrPtr = &chunkManager]() {
            if (tilesPtr) {
                if (itemToDrop == ecs::HeldItem::Wood) world::ResourceSystem::spawnLogs(registry, **tilesPtr, {wpos.wx, wpos.wy}, wpos.wz, 1, chunkMgrPtr, entt::null, true, itemSubtype);
                else world::ResourceSystem::spawnSmallRocks(registry, **tilesPtr, {wpos.wx, wpos.wy}, wpos.wz, 1, chunkMgrPtr, entt::null, true, itemSubtype);
            }
        });

        anim.currentAnim = 2;
        anim.currentFrame = 0;
        anim.elapsedTime = 0.0f;
        anim.isActionLocked = true;
        anim.damageDealt = true;
        anim.numFrames = 4;
    }
}

}