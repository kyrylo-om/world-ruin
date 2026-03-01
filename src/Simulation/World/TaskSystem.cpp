#include "../../../include/Simulation/World/TaskSystem.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include <algorithm>

namespace wr::simulation {

    void TaskSystem::update(entt::registry& registry) {
        auto taskView = registry.view<ecs::TaskArea>();
        auto markedView = registry.view<ecs::MarkedForHarvestTag, ecs::WorldPos>();
        auto workerView = registry.view<ecs::WorkerTag, ecs::UnitData>();

        std::vector<entt::entity> completedTasks;

        for (auto taskEnt : taskView) {
            bool hasResourcesLeft = false;

            for (auto resEnt : markedView) {
                if (markedView.get<ecs::MarkedForHarvestTag>(resEnt).taskEntity == taskEnt) {
                    hasResourcesLeft = true;
                    break;
                }
            }

            bool hasActiveWorkers = false;

            for (auto wEnt : workerView) {
                auto& uData = workerView.get<ecs::UnitData>(wEnt);
                if (uData.currentTask == taskEnt) {

                    bool isMovingToTarget = registry.all_of<ecs::ActionTarget>(wEnt);
                    bool isCarryingItem = uData.heldItem != ecs::HeldItem::None;

                    if (isMovingToTarget || isCarryingItem) {
                        hasActiveWorkers = true;
                        break;
                    }
                }
            }

            if (!hasResourcesLeft && !hasActiveWorkers) {
                completedTasks.push_back(taskEnt);
            }
        }

        for (auto e : completedTasks) {
            for (auto resEnt : markedView) {
                if (markedView.get<ecs::MarkedForHarvestTag>(resEnt).taskEntity == e) {
                    registry.remove<ecs::MarkedForHarvestTag>(resEnt);
                    registry.remove<ecs::ClaimedTag>(resEnt);
                }
            }
            registry.destroy(e);
        }
    }

}