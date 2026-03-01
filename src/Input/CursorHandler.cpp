#include "Input/CursorHandler.hpp"
#include "Input/CursorRaycast.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include "Rendering/TileHandler.hpp"
#include <SFML/Window/Mouse.hpp>
#include <cmath>

namespace wr::input {

void CursorHandler::init(const sf::Texture& texture) noexcept {
    m_sprite.setTexture(texture);
    m_sprite.setOrigin(24.0f, 16.0f);
}

    void CursorHandler::update(const sf::RenderWindow& window, const rendering::Camera& camera, world::ChunkManager& chunkManager, entt::registry& registry) noexcept {
    sf::Vector2i pixelPos = sf::Mouse::getPosition(window);

    sf::Vector2f uiPos = window.mapPixelToCoords(pixelPos, window.getDefaultView());
    m_sprite.setPosition(uiPos);

    m_exactHoveredWorldPos = CursorRaycast::getHoveredWorldPos(window, camera, chunkManager);

    m_taskSystem.update(registry);

    bool hoveringInteractable = false;
    sf::Vector2f viewPos = window.mapPixelToCoords(pixelPos, camera.getView());

    auto unitView = registry.view<ecs::ScreenPos, ecs::WorldPos, ecs::UnitTag>();
    for (auto entity : unitView) {
        const auto& sp = unitView.get<ecs::ScreenPos>(entity);
        const auto& wp = unitView.get<ecs::WorldPos>(entity);
        float clickRad = 32.0f;
        if (const auto* ca = registry.try_get<ecs::ClickArea>(entity)) clickRad = ca->radius;
        float dx = viewPos.x - sp.x;
        float dy = viewPos.y - (sp.y - wp.wz - 32.0f);
        if (dx * dx + dy * dy <= clickRad * clickRad) { hoveringInteractable = true; break; }
    }

    if (!hoveringInteractable) {
        auto resView = registry.view<ecs::ScreenPos, ecs::WorldPos, ecs::ResourceTag>();
        for (auto entity : resView) {
            const auto& sp = registry.get<ecs::ScreenPos>(entity);
            const auto& wp = registry.get<ecs::WorldPos>(entity);
            float clickRad = 19.2f;
            if (const auto* ca = registry.try_get<ecs::ClickArea>(entity)) clickRad = ca->radius;
            else if (const auto* hb = registry.try_get<ecs::Hitbox>(entity)) clickRad = hb->radius;
            float dx = viewPos.x - sp.x;
            float dy = viewPos.y - (sp.y - wp.wz);
            if (dx * dx + dy * dy <= clickRad * clickRad) { hoveringInteractable = true; break; }
        }
    }

    const auto* tilesPtr = registry.ctx().find<const rendering::TileHandler*>();
    if (tilesPtr) {
        if (hoveringInteractable) m_sprite.setTexture((*tilesPtr)->getCursorHoverTexture());
        else                      m_sprite.setTexture((*tilesPtr)->getCursorTexture());
    }
}

bool CursorHandler::handleMouseClick(entt::registry& registry, const sf::RenderWindow& window, const rendering::Camera& camera, world::ChunkManager& chunkManager) noexcept {
    if (m_taskSystem.hasPendingTask()) return false;
    return CursorClickSystem::handleMouseClick(registry, window, camera);
}

void CursorHandler::startDrag() noexcept {

    if (m_taskSystem.hasPendingTask() && !m_taskSystem.isSelectingDropoff()) return;
    m_dragSystem.startDrag(m_exactHoveredWorldPos);
}

bool CursorHandler::endDrag(entt::registry& registry, const rendering::Camera& camera, world::ChunkManager& chunkManager) noexcept {
    return m_dragSystem.endDrag(registry, chunkManager, m_exactHoveredWorldPos, m_taskSystem);
}

void CursorHandler::draw(sf::RenderWindow& window) const {
    window.setView(window.getDefaultView());
    window.draw(m_sprite);
}

void CursorHandler::setSystemCursorVisible(sf::RenderWindow& window, bool visible) const noexcept {
    window.setMouseCursorVisible(visible);
}

}