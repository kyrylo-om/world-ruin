#include "Simulation/World/OverseerSystem.hpp"
#include "Input/CursorTaskSystem/TaskFinalizer.hpp"
#include "Core/SimLogger.hpp"
#include "ECS/Tags.hpp"

namespace wr::simulation {

entt::entity OverseerSystem::createTask(entt::registry& registry,
                                         math::Vec2f areaStart, math::Vec2f areaEnd,
                                         bool collectFutureDrops, bool useCityStorage,
                                         bool hasDropoff, math::Vec2f dropoffStart, math::Vec2f dropoffEnd) {
    // Collect idle workers and determine what they can harvest (same logic as player mode)
    bool canTrees = false, canRocks = false, canSmallRocks = false, canBushes = false, canLogs = false;
    std::vector<entt::entity> workers;
    entt::entity spareCourier = entt::null;

    auto view = registry.view<ecs::UnitData, ecs::WorkerState>();
    for (auto w : view) {
        if (registry.all_of<ecs::WorkerTag>(w)) continue;
        auto& uData = view.get<ecs::UnitData>(w);
        auto& wState = view.get<ecs::WorkerState>(w);
        if (wState.currentTask != entt::null) continue;
        if (registry.all_of<ecs::PathRequest>(w) || registry.all_of<ecs::Path>(w)) continue;

        switch (uData.type) {
            case ecs::UnitType::Lumberjack: canTrees = true; canBushes = true; workers.push_back(w); break;
            case ecs::UnitType::Miner:      canRocks = true; workers.push_back(w); break;
            case ecs::UnitType::Warrior:    canBushes = true; workers.push_back(w); break;
            case ecs::UnitType::Courier:
                canLogs = true; canSmallRocks = true;
                if (spareCourier == entt::null) spareCourier = w;
                break;
            default: break;
        }
    }

    // One courier per task for auto-collection
    if (spareCourier != entt::null) workers.push_back(spareCourier);
    if (workers.empty()) return entt::null;

    // Mark ALL resource types regardless of which workers are currently available.
    // Workers self-filter via canHarvest — this ensures that when a lumberjack
    // joins a clearing task later, trees are already marked and ready to harvest.
    canTrees = true; canRocks = true; canSmallRocks = true; canBushes = true; canLogs = true;

    // Build PendingTaskArea with include flags
    ecs::PendingTaskArea pArea;
    pArea.areas.push_back(ecs::RectArea{
        areaStart, areaEnd,
        canTrees, canRocks, canSmallRocks, canBushes, canLogs,
        canTrees, canRocks, canSmallRocks, canBushes, canLogs
    });
    pArea.collectFutureDrops = collectFutureDrops;
    pArea.useCityStorage = useCityStorage;
    pArea.hasDropoff = hasDropoff;
    pArea.dropoffStart = dropoffStart;
    pArea.dropoffEnd = dropoffEnd;

    // Use the shared task system pipeline
    registry.clear<ecs::PreviewHarvestTag>();
    input::TaskFinalizer::scanResources(registry, pArea);
    auto taskEnt = input::TaskFinalizer::finalizeTask(registry, pArea, m_globalTaskId, workers);
    registry.clear<ecs::PreviewHarvestTag>();

    return taskEnt;
}

}
