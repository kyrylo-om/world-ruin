#include "Input/CursorClickSystem.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include <SFML/Window/Mouse.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <algorithm>

namespace wr::input {

bool CursorClickSystem::handleMouseClick(entt::registry& registry, const sf::RenderWindow& window, const rendering::Camera& camera) {
    sf::Vector2i pixelPos = sf::Mouse::getPosition(window);
    sf::Vector2f viewPos = window.mapPixelToCoords(pixelPos, camera.getView());

    bool isCtrl = sf::Keyboard::isKeyPressed(sf::Keyboard::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::RControl);

    entt::entity clickedUnit = entt::null;
    float closestUnitDistSq = 999999.0f;

    auto unitView = registry.view<ecs::ScreenPos, ecs::WorldPos, ecs::UnitTag>();
    for (auto entity : unitView) {
        const auto& sp = unitView.get<ecs::ScreenPos>(entity);
        const auto& wp = unitView.get<ecs::WorldPos>(entity);
        float clickRad = 32.0f;
        if (const auto* ca = registry.try_get<ecs::ClickArea>(entity)) clickRad = ca->radius;
        float dx = viewPos.x - sp.x;
        float dy = viewPos.y - (sp.y - wp.wz - 32.0f);
        float distSq = dx * dx + dy * dy;
        if (distSq <= clickRad * clickRad && distSq < closestUnitDistSq) {
            closestUnitDistSq = distSq; clickedUnit = entity;
        }
    }

    if (clickedUnit != entt::null) {
        if (!isCtrl) registry.clear<ecs::SelectedTag>();

        if (isCtrl && registry.all_of<ecs::SelectedTag>(clickedUnit)) {
            registry.remove<ecs::SelectedTag>(clickedUnit);
        } else {
            registry.emplace_or_replace<ecs::SelectedTag>(clickedUnit);
            registry.remove<ecs::IdleTag>(clickedUnit);
        }
        return true;
    }

    if (registry.ctx().get<ecs::GameMode>() == ecs::GameMode::Player) {

        entt::entity clickedRes = entt::null;
        float closestResDistSq = 999999.0f;

        bool canHarvestTrees = false, canHarvestRocks = false, canHarvestBushes = false, canHarvestLogs = false, canHarvestSmallRocks = false;
        bool hasSelection = false;

        auto selView = registry.view<ecs::SelectedTag, ecs::UnitData>();
        for (auto e : selView) {
            hasSelection = true;
            auto type = selView.get<ecs::UnitData>(e).type;
            if (type == ecs::UnitType::Warrior) { canHarvestBushes = true; }
            if (type == ecs::UnitType::Lumberjack) { canHarvestTrees = true; canHarvestBushes = true; }
            if (type == ecs::UnitType::Miner) { canHarvestRocks = true; }
            if (type == ecs::UnitType::Courier) { canHarvestLogs = true; canHarvestSmallRocks = true; }
        }

        if (hasSelection) {
            auto resView = registry.view<ecs::ScreenPos, ecs::WorldPos>(entt::exclude<ecs::UnitTag>);
            for (auto entity : resView) {
                const auto& sp = resView.get<ecs::ScreenPos>(entity);
                const auto& wp = resView.get<ecs::WorldPos>(entity);
                float clickRad = 19.2f;
                if (const auto* ca = registry.try_get<ecs::ClickArea>(entity)) clickRad = ca->radius;
                else if (const auto* hb = registry.try_get<ecs::Hitbox>(entity)) clickRad = hb->radius;

                float dx = viewPos.x - sp.x;
                float dy = viewPos.y - (sp.y - wp.wz);
                float distSq = dx * dx + dy * dy;

                if (distSq <= clickRad * clickRad && distSq < closestResDistSq) {
                    bool interactable = false;
                    if (registry.all_of<ecs::TreeTag>(entity) && canHarvestTrees) interactable = true;
                    else if (registry.all_of<ecs::RockTag>(entity) && canHarvestRocks) interactable = true;
                    else if (registry.all_of<ecs::BushTag>(entity) && canHarvestBushes) interactable = true;
                    else if (registry.all_of<ecs::LogTag>(entity) && canHarvestLogs) interactable = true;
                    else if (registry.all_of<ecs::SmallRockTag>(entity) && canHarvestSmallRocks) interactable = true;

                    if (interactable) {
                        closestResDistSq = distSq; clickedRes = entity;
                    }
                }
            }
        }

        if (clickedRes != entt::null) {
            auto selectedUnits = registry.view<ecs::SelectedTag>();
            bool anyAssigned = false;

            for (auto e : selectedUnits) {
                auto& uData = registry.get<ecs::UnitData>(e);
                bool canHelp = false;

                if (registry.all_of<ecs::TreeTag>(clickedRes) && uData.type == ecs::UnitType::Lumberjack) canHelp = true;
                else if (registry.all_of<ecs::RockTag>(clickedRes) && uData.type == ecs::UnitType::Miner) canHelp = true;
                else if (registry.all_of<ecs::BushTag>(clickedRes) && (uData.type == ecs::UnitType::Lumberjack || uData.type == ecs::UnitType::Warrior)) canHelp = true;
                else if (registry.all_of<ecs::LogTag>(clickedRes) && uData.type == ecs::UnitType::Courier) canHelp = true;
                else if (registry.all_of<ecs::SmallRockTag>(clickedRes) && uData.type == ecs::UnitType::Courier) canHelp = true;

                if (canHelp) {
                    registry.emplace_or_replace<ecs::WorkerTag>(e);
                    registry.remove<ecs::Path>(e);

                    if (!isCtrl) {
                        if (auto* oldTarget = registry.try_get<ecs::ActionTarget>(e)) {
                            if (registry.valid(oldTarget->target)) registry.remove<ecs::ClaimedTag>(oldTarget->target);
                        }
                    }
                    registry.emplace_or_replace<ecs::ActionTarget>(e, clickedRes);
                    registry.remove<ecs::IdleTag>(e);
                    anyAssigned = true;
                }
            }

            if (anyAssigned && !registry.all_of<ecs::BuildingTag>(clickedRes)) {
                registry.emplace_or_replace<ecs::MarkedForHarvestTag>(clickedRes, sf::Color::White);
            }

            return anyAssigned;
        }
    } else {

        if (!isCtrl) registry.clear<ecs::SelectedTag>();
    }

    return false;
}

}