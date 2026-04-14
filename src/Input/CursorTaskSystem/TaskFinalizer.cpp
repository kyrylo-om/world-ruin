#include "../../../../include/Input/CursorTaskSystem/TaskFinalizer.hpp"
#include "ECS/Tags.hpp"

namespace wr::input {

void TaskFinalizer::cancelTask(entt::registry& registry) {
    registry.clear<ecs::PreviewHarvestTag>();
    registry.clear<ecs::PendingTaskArea>();
}

void TaskFinalizer::finalizeTask(entt::registry& registry, ecs::PendingTaskArea& pArea, int& globalTaskId) {
    auto previewView = registry.view<ecs::PreviewHarvestTag>();
    bool markedAny = false;

    int wood = 0, rock = 0, smallRock = 0, bush = 0, log = 0;
    for (auto e : previewView) {
        if (registry.all_of<ecs::TreeTag>(e)) wood++;
        if (registry.all_of<ecs::RockTag>(e)) rock++;
        if (registry.all_of<ecs::SmallRockTag>(e)) smallRock++;
        if (registry.all_of<ecs::BushTag>(e)) bush++;
        if (registry.all_of<ecs::LogTag>(e)) log++;
    }

    sf::Color randomColor(rand() % 155 + 100, rand() % 155 + 100, rand() % 155 + 100, 80);
    sf::Color solidColor(randomColor.r, randomColor.g, randomColor.b, 255);

    auto areaEntity = registry.create();
    registry.emplace<ecs::TaskArea>(areaEntity, pArea.areas, pArea.targetBuildings, randomColor, globalTaskId++,
                                    pArea.hasDropoff, pArea.dropoffStart, pArea.dropoffEnd, pArea.collectFutureDrops, pArea.useCityStorage);

    for (auto e : previewView) {
        registry.emplace_or_replace<ecs::MarkedForHarvestTag>(e, solidColor, areaEntity);
        markedAny = true;
    }

    bool hasHarvestWork = wood > 0 || rock > 0 || bush > 0;
    bool courierOnlyTask = !hasHarvestWork && (pArea.collectFutureDrops || log > 0 || smallRock > 0 || !pArea.targetBuildings.empty());

    if (markedAny || pArea.hasDropoff || !pArea.targetBuildings.empty() || courierOnlyTask) {
        auto selectedUnits = registry.view<ecs::SelectedTag, ecs::UnitData, ecs::WorkerState>();
        for (auto e : selectedUnits) {
            auto& uData = selectedUnits.get<ecs::UnitData>(e);
            bool canHelp = false;

            if (courierOnlyTask) {

                if (uData.type == ecs::UnitType::Courier) {
                    bool hasDestination = pArea.hasDropoff || !pArea.targetBuildings.empty() || pArea.useCityStorage;
                    if (hasDestination) canHelp = true;
                }
            } else {
                if (uData.type == ecs::UnitType::Lumberjack && (wood > 0 || bush > 0)) canHelp = true;
                if (uData.type == ecs::UnitType::Miner && rock > 0) canHelp = true;
                if (uData.type == ecs::UnitType::Warrior && bush > 0) canHelp = true;
                if (uData.type == ecs::UnitType::Courier) {
                    bool hasDestination = pArea.hasDropoff || !pArea.targetBuildings.empty() || pArea.useCityStorage;
                    if (hasDestination && (pArea.collectFutureDrops || log > 0 || smallRock > 0 || !pArea.targetBuildings.empty()))
                        canHelp = true;
                }
            }

            if (canHelp) {
                registry.get<ecs::WorkerState>(e).currentTask = areaEntity;
                registry.emplace_or_replace<ecs::WorkerTag>(e);
                registry.remove<ecs::IdleTag>(e);
            }
        }
    } else {
        registry.destroy(areaEntity);
    }
}

}