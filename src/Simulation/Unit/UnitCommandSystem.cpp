#include "../../../include/Simulation/Unit/UnitCommandSystem.hpp"
#include "../../../include/Simulation/Unit/UnitTaskSystem.hpp"
#include "Core/ThreadPool.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include <cmath>
#include <algorithm>

namespace wr::simulation {

void UnitCommandSystem::processCommands(entt::registry& registry, world::ChunkManager& chunkManager,
                                        bool pJustPressed, bool isCancelCommand,
                                        bool isRightClicking, const math::Vec2f& mouseWorldPos,
                                        std::shared_ptr<GlobalObstacleMap> obstacles) {

    auto selectedView = registry.view<ecs::SelectedTag>();
    int selectedCount = std::distance(selectedView.begin(), selectedView.end());

    if (pJustPressed) {
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

    std::vector<entt::entity> movableUnits;
    if (selectedCount > 0) {
        for (auto entity : selectedView) {
            bool hasTask = registry.all_of<ecs::WorkerTag>(entity) || (registry.all_of<ecs::WorkerState>(entity) && registry.get<ecs::WorkerState>(entity).currentTask != entt::null);
            bool isPaused = registry.all_of<ecs::PausedTag>(entity);
            if (hasTask && !isPaused) continue;
            movableUnits.push_back(entity);
        }
    }

    if (isRightClicking && !movableUnits.empty()) {
        int mCount = movableUnits.size();
        int side = std::max(1, static_cast<int>(std::ceil(std::sqrt(mCount))));
        int idx = 0;
        float spacing = 1.0f;

        auto* threadPool = registry.ctx().get<core::ThreadPool*>();

        for (auto entity : movableUnits) {
            auto& wp = registry.get<ecs::WorldPos>(entity);

            float offsetX = ((idx % side) - (side - 1) / 2.0f) * spacing;
            float offsetY = ((idx / side) - (side - 1) / 2.0f) * spacing;
            math::Vec2f targetWorld = {mouseWorldPos.x + offsetX, mouseWorldPos.y + offsetY};

            auto pathFuture = threadPool->enqueue([startWp = math::Vec2f{wp.wx, wp.wy}, targetWorld, &chunkManager, obstacles]() {
                return PathfindingSystem::findPath(startWp, targetWorld, chunkManager, *obstacles);
            });
            registry.emplace_or_replace<ecs::PathRequest>(entity, std::move(pathFuture));
            registry.remove<ecs::IdleTag>(entity);

            bool hasTask = registry.all_of<ecs::WorkerTag>(entity) || (registry.all_of<ecs::WorkerState>(entity) && registry.get<ecs::WorkerState>(entity).currentTask != entt::null);

            if (!hasTask) {
                if (registry.any_of<ecs::ActionTarget>(entity)) {
                    UnitTaskSystem::releaseUnitAction(registry, entity);
                }
            }
            idx++;
        }
    }
}

}