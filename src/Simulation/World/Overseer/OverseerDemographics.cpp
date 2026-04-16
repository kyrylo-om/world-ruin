#include "Simulation/World/OverseerSystem.hpp"
#include "ECS/Tags.hpp"
#include "Config/UnitConfig.hpp"
#include "Config/BuildingsConfig.hpp"
#include "Rendering/TileHandler.hpp"
#include "Core/SimLogger.hpp"
#include <cmath>
#include <algorithm>
#include <string>

namespace wr::simulation {

void OverseerSystem::processDemographics(entt::registry& registry) {
    auto& board = registry.ctx().get<SimBlackboard>();
    auto& simLog = core::SimLogger::get();

    // ─── Housing capacity — only BUILT houses count ───
    int builtHouses = 0;
    auto capacityView = registry.view<ecs::BuildingTag, ecs::ConstructionData>(entt::exclude<ecs::CityStorageTag>);
    for (auto e : capacityView) {
        if (capacityView.get<ecs::ConstructionData>(e).isBuilt) builtHouses++;
    }
    int housingCapacity = builtHouses * config::UNITS_PER_HOUSE;
    int spawnBudget = std::max(0, housingCapacity - board.totalUnits);

    // ─── Find newly built houses ───
    auto bView = registry.view<ecs::BuildingTag, ecs::ConstructionData, ecs::WorldPos>(entt::exclude<SpawnedFlag>);

    for (auto e : bView) {
        if (!bView.get<ecs::ConstructionData>(e).isBuilt) continue;
        registry.emplace<SpawnedFlag>(e);

        int toSpawn = std::min(spawnBudget, config::UNITS_PER_HOUSE);
        if (toSpawn <= 0) {
            simLog.log("[Demographics] House #" + std::to_string(core::SimLogger::eid(e))
                + " built, but at capacity (" + std::to_string(board.totalUnits)
                + "/" + std::to_string(housingCapacity) + ") — no spawn");
            continue;
        }

        auto& wp = bView.get<ecs::WorldPos>(e);

        for (int i = 0; i < toSpawn; ++i) {
            // ═══════════════════════════════════════════════════════
            //  Bottleneck analysis — score each unit type by need
            // ═══════════════════════════════════════════════════════

            // Count workload in active tasks
            int totalTrees = 0, totalRockNodes = 0, totalDrops = 0;
            auto markedView = registry.view<ecs::MarkedForHarvestTag>();
            for (auto m : markedView) {
                if (registry.all_of<ecs::TreeTag>(m)) totalTrees++;
                else if (registry.all_of<ecs::RockTag>(m)) totalRockNodes++;
                else if (registry.all_of<ecs::LogTag>(m) || registry.all_of<ecs::SmallRockTag>(m)) totalDrops++;
            }

            float scores[4] = {0.0f, 0.0f, 0.0f, 0.0f}; // [0]=lumberjack [1]=miner [2]=courier [3]=builder

            // --- Harvesting backlog: trees per lumberjack ---
            if (board.popLumberjack > 0) {
                scores[0] += static_cast<float>(totalTrees) / static_cast<float>(board.popLumberjack);
            } else if (totalTrees > 0 || board.demandWood > board.availWood) {
                scores[0] += 10.0f; // Critical: trees to cut but no lumberjacks
            }

            // --- Harvesting backlog: rocks per miner ---
            if (board.popMiner > 0) {
                scores[1] += static_cast<float>(totalRockNodes) / static_cast<float>(board.popMiner);
            } else if (totalRockNodes > 0 || board.demandRock > board.availRock) {
                scores[1] += 10.0f;
            }

            // --- Delivery backlog: drops per courier ---
            if (board.popCourier > 0) {
                scores[2] += static_cast<float>(totalDrops) / static_cast<float>(board.popCourier);
                scores[2] += board.averageDropoffDistance / (static_cast<float>(board.popCourier) * 4.5f);
            } else if (totalDrops > 0) {
                scores[2] += 10.0f;
            }

            // --- Courier-to-harvester ratio: need ~1 per 2 harvesters ---
            int totalHarvesters = board.popLumberjack + board.popMiner;
            float idealCouriers = std::ceil(totalHarvesters * 0.5f);
            if (static_cast<float>(board.popCourier) < idealCouriers) {
                scores[2] += (idealCouriers - static_cast<float>(board.popCourier)) * 3.0f;
            }

            // --- Construction demand ---
            if (board.pendingBuildings > 0) {
                if (board.popBuilder > 0)
                    scores[3] += static_cast<float>(board.pendingBuildings) / static_cast<float>(board.popBuilder);
                else
                    scores[3] += 10.0f;
            }

            // --- Resource deficit adds to harvester scores ---
            if (board.demandWood > board.availWood) {
                scores[0] += static_cast<float>(board.demandWood - board.availWood)
                    / static_cast<float>(board.popLumberjack + 1) * 0.3f;
            }
            if (board.demandRock > board.availRock) {
                scores[1] += static_cast<float>(board.demandRock - board.availRock)
                    / static_cast<float>(board.popMiner + 1) * 0.3f;
            }

            // Pick the type with highest score
            int best = 0;
            for (int j = 1; j < 4; j++) {
                if (scores[j] > scores[best]) best = j;
            }

            ecs::UnitType chosenType;
            switch (best) {
                case 0: chosenType = ecs::UnitType::Lumberjack; break;
                case 1: chosenType = ecs::UnitType::Miner; break;
                case 2: chosenType = ecs::UnitType::Courier; break;
                case 3: chosenType = ecs::UnitType::Builder; break;
                default: chosenType = ecs::UnitType::Lumberjack; break;
            }

            // ═══════════════════════════════════════════════════════
            //  Spawn the unit
            // ═══════════════════════════════════════════════════════

            auto unitEnt = registry.create();
            registry.emplace<ecs::LogicalPos>(unitEnt, static_cast<int64_t>(wp.wx), static_cast<int64_t>(wp.wy));
            registry.emplace<ecs::WorldPos>(unitEnt, wp.wx + (i * 1.5f), wp.wy + 1.5f, wp.wz, wp.wz);
            registry.emplace<ecs::ScreenPos>(unitEnt);
            registry.emplace<ecs::Velocity>(unitEnt);
            registry.emplace<ecs::UnitTag>(unitEnt);
            registry.emplace<ecs::WorkerState>(unitEnt);
            auto& spr = registry.emplace<ecs::SpriteComponent>(unitEnt).sprite;
            registry.emplace<ecs::AnimationState>(unitEnt, 0.0f, 0.1f, 192, 192, 6, 0);

            spr.setOrigin(96.0f, 112.0f);
            spr.setTextureRect(sf::IntRect(0, 0, 192, 192));

            auto* tilesPtr = registry.ctx().find<const rendering::TileHandler*>();

            switch (chosenType) {
                case ecs::UnitType::Lumberjack:
                    registry.emplace<ecs::UnitData>(unitEnt, ecs::UnitType::Lumberjack, ecs::HeldItem::None, uint8_t{0});
                    registry.emplace<ecs::Health>(unitEnt, config::LUMBERJACK_HP, config::LUMBERJACK_HP);
                    registry.emplace<ecs::CombatStats>(unitEnt, config::LUMBERJACK_DAMAGE, config::LUMBERJACK_RANGE, config::LUMBERJACK_ARC);
                    if (tilesPtr) spr.setTexture((*tilesPtr)->getPawnIdleAxeTexture());
                    board.popLumberjack++;
                    break;
                case ecs::UnitType::Miner:
                    registry.emplace<ecs::UnitData>(unitEnt, ecs::UnitType::Miner, ecs::HeldItem::None, uint8_t{0});
                    registry.emplace<ecs::Health>(unitEnt, config::MINER_HP, config::MINER_HP);
                    registry.emplace<ecs::CombatStats>(unitEnt, config::MINER_DAMAGE, config::MINER_RANGE, config::MINER_ARC);
                    if (tilesPtr) spr.setTexture((*tilesPtr)->getPawnIdlePickaxeTexture());
                    board.popMiner++;
                    break;
                case ecs::UnitType::Courier:
                    registry.emplace<ecs::UnitData>(unitEnt, ecs::UnitType::Courier, ecs::HeldItem::None, uint8_t{0});
                    registry.emplace<ecs::Health>(unitEnt, config::COURIER_HP, config::COURIER_HP);
                    registry.emplace<ecs::CombatStats>(unitEnt, config::COURIER_DAMAGE, config::COURIER_RANGE, config::COURIER_ARC);
                    if (tilesPtr) spr.setTexture((*tilesPtr)->getPawnIdleTexture());
                    board.popCourier++;
                    break;
                case ecs::UnitType::Builder:
                    registry.emplace<ecs::UnitData>(unitEnt, ecs::UnitType::Builder, ecs::HeldItem::None, uint8_t{0});
                    registry.emplace<ecs::Health>(unitEnt, config::BUILDER_HP, config::BUILDER_HP);
                    registry.emplace<ecs::CombatStats>(unitEnt, config::BUILDER_DAMAGE, config::BUILDER_RANGE, config::BUILDER_ARC);
                    if (tilesPtr) spr.setTexture((*tilesPtr)->getPawnIdleHammerTexture());
                    board.popBuilder++;
                    break;
                default: break;
            }
            board.totalUnits++;

            registry.emplace<ecs::SelectedTag>(unitEnt);

            simLog.log("[Demographics] Spawned " + std::string(core::SimLogger::typeName(chosenType))
                + " #" + std::to_string(core::SimLogger::eid(unitEnt))
                + " at " + core::SimLogger::pos(wp.wx + (i * 1.5f), wp.wy + 1.5f)
                + " (scores: L=" + std::to_string(static_cast<int>(scores[0] * 10))
                + " M=" + std::to_string(static_cast<int>(scores[1] * 10))
                + " C=" + std::to_string(static_cast<int>(scores[2] * 10))
                + " B=" + std::to_string(static_cast<int>(scores[3] * 10))
                + ") pop=" + std::to_string(board.totalUnits) + "/" + std::to_string(housingCapacity));
        }

        spawnBudget -= toSpawn;
    }
}

}
