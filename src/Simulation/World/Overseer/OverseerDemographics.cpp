#include "Simulation/World/OverseerSystem.hpp"
#include "ECS/Tags.hpp"
#include "Config/UnitConfig.hpp"
#include "Rendering/TileHandler.hpp"
#include <cmath>
#include <algorithm>
#include <vector>
#include <iostream>

namespace wr::simulation {

void OverseerSystem::processDemographics(entt::registry& registry) {
    auto& board = registry.ctx().get<SimBlackboard>();
    auto bView = registry.view<ecs::BuildingTag, ecs::ConstructionData, ecs::WorldPos>(entt::exclude<SpawnedFlag>);

    for (auto e : bView) {
        if (bView.get<ecs::ConstructionData>(e).isBuilt) {
            registry.emplace<SpawnedFlag>(e);
            auto& wp = bView.get<ecs::WorldPos>(e);

            float wMiner = std::max(0, board.demandRock - board.availRock) / static_cast<float>(board.popMiner + 1);
            float wLumb  = std::max(0, board.demandWood - board.availWood) / static_cast<float>(board.popLumberjack + 1);

            float baseSpeed = 4.5f;
            float wCour = board.averageDropoffDistance / (static_cast<float>(board.popCourier + 1) * baseSpeed);

            float wBldr = static_cast<float>(board.pendingBuildings * 2) / static_cast<float>(board.popBuilder + 1);

            for (int i = 0; i < 2; ++i) {
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

                if (wLumb >= wMiner && wLumb >= wCour && wLumb >= wBldr) {
                    registry.emplace<ecs::UnitData>(unitEnt, ecs::UnitType::Lumberjack, ecs::HeldItem::None, uint8_t{0});
                    registry.emplace<ecs::Health>(unitEnt, config::LUMBERJACK_HP, config::LUMBERJACK_HP);
                    registry.emplace<ecs::CombatStats>(unitEnt, config::LUMBERJACK_DAMAGE, config::LUMBERJACK_RANGE, config::LUMBERJACK_ARC);
                    if (tilesPtr) spr.setTexture((*tilesPtr)->getPawnIdleAxeTexture());
                    wLumb /= 2.0f;
                    std::cout << "[Demographics] Spawned Lumberjack." << std::endl;
                } else if (wMiner >= wCour && wMiner >= wBldr) {
                    registry.emplace<ecs::UnitData>(unitEnt, ecs::UnitType::Miner, ecs::HeldItem::None, uint8_t{0});
                    registry.emplace<ecs::Health>(unitEnt, config::MINER_HP, config::MINER_HP);
                    registry.emplace<ecs::CombatStats>(unitEnt, config::MINER_DAMAGE, config::MINER_RANGE, config::MINER_ARC);
                    if (tilesPtr) spr.setTexture((*tilesPtr)->getPawnIdlePickaxeTexture());
                    wMiner /= 2.0f;
                    std::cout << "[Demographics] Spawned Miner." << std::endl;
                } else if (wCour >= wBldr) {
                    registry.emplace<ecs::UnitData>(unitEnt, ecs::UnitType::Courier, ecs::HeldItem::None, uint8_t{0});
                    registry.emplace<ecs::Health>(unitEnt, config::COURIER_HP, config::COURIER_HP);
                    registry.emplace<ecs::CombatStats>(unitEnt, config::COURIER_DAMAGE, config::COURIER_RANGE, config::COURIER_ARC);
                    if (tilesPtr) spr.setTexture((*tilesPtr)->getPawnIdleTexture());
                    wCour /= 2.0f;
                    std::cout << "[Demographics] Spawned Courier (Avg Dropoff Dist: " << board.averageDropoffDistance << ")." << std::endl;
                } else {
                    registry.emplace<ecs::UnitData>(unitEnt, ecs::UnitType::Builder, ecs::HeldItem::None, uint8_t{0});
                    registry.emplace<ecs::Health>(unitEnt, config::BUILDER_HP, config::BUILDER_HP);
                    registry.emplace<ecs::CombatStats>(unitEnt, config::BUILDER_DAMAGE, config::BUILDER_RANGE, config::BUILDER_ARC);
                    if (tilesPtr) spr.setTexture((*tilesPtr)->getPawnIdleHammerTexture());
                    wBldr /= 2.0f;
                    std::cout << "[Demographics] Spawned Builder." << std::endl;
                }

                registry.emplace<ecs::SelectedTag>(unitEnt);
            }
        }
    }
}

}