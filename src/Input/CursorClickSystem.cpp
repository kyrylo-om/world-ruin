#include "Input/CursorClickSystem.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include <SFML/Window/Mouse.hpp>

namespace wr::input {

bool CursorClickSystem::handleMouseClick(entt::registry& registry, const sf::RenderWindow& window, const rendering::Camera& camera) {
    sf::Vector2i pixelPos = sf::Mouse::getPosition(window);
    sf::Vector2f viewPos = window.mapPixelToCoords(pixelPos, camera.getView());

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

    entt::entity clickedRes = entt::null;
    float closestResDistSq = 999999.0f;

    auto resView = registry.view<ecs::ScreenPos, ecs::WorldPos, ecs::ResourceTag>();
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
            closestResDistSq = distSq; clickedRes = entity;
        }
    }

    if (clickedUnit != entt::null) {
        registry.clear<ecs::SelectedTag>();
        registry.emplace<ecs::SelectedTag>(clickedUnit);
        return true;
    }
    else if (clickedRes != entt::null) {
        auto selectedUnits = registry.view<ecs::SelectedTag>();
        if (selectedUnits.begin() != selectedUnits.end()) {
            registry.emplace_or_replace<ecs::MarkedForHarvestTag>(clickedRes, sf::Color::White);
            for (auto e : selectedUnits) {
                if (auto* oldTarget = registry.try_get<ecs::ActionTarget>(e)) {
                    if (registry.valid(oldTarget->target)) registry.remove<ecs::ClaimedTag>(oldTarget->target);
                }
                registry.emplace_or_replace<ecs::ActionTarget>(e, clickedRes);
                registry.emplace_or_replace<ecs::WorkerTag>(e);
                registry.remove<ecs::Path>(e);
            }
            return true;
        }
        return false;
    }
    return false;
}

}