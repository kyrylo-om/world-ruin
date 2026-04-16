#include "../../include/Input/CursorBuilderSystem.hpp"
#include "../../include/Config/BuildingsConfig.hpp"
#include "Simulation/BuildingPlacement.hpp"
#include "ECS/Components.hpp"
#include <SFML/Window/Keyboard.hpp>

namespace wr::input {

void CursorBuilderSystem::update(entt::registry& registry, world::ChunkManager& chunkManager, const math::Vec2f& hoveredPos) {
    bool currentB = sf::Keyboard::isKeyPressed(sf::Keyboard::B);
    bool current1 = sf::Keyboard::isKeyPressed(sf::Keyboard::Num1);
    bool current2 = sf::Keyboard::isKeyPressed(sf::Keyboard::Num2);
    bool currentEnter = sf::Keyboard::isKeyPressed(sf::Keyboard::Enter);
    bool isCtrl = sf::Keyboard::isKeyPressed(sf::Keyboard::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::RControl);

    if (currentB && !m_prevB && !isCtrl) {
        m_builderModeActive = !m_builderModeActive;
        if (m_builderModeActive) {
            if (registry.ctx().contains<ecs::TaskModeState>()) {
                registry.ctx().get<ecs::TaskModeState>().active = false;
            }

        } else {
            m_selectedBuilding = 0;
            m_canPlace = false;
        }
    }

    if (!m_builderModeActive) {
        m_prevB = currentB; m_prev1 = current1; m_prevEnter = currentEnter;
        if (registry.ctx().contains<ecs::BuilderModeState>()) {
            registry.ctx().get<ecs::BuilderModeState>().active = false;
        }
        return;
    }

    if (current1 && !m_prev1) m_selectedBuilding = 1;
    if (current2) m_selectedBuilding = 2;

    m_placementPos = hoveredPos;
    m_canPlace = false;

    if (m_selectedBuilding == 1 || m_selectedBuilding == 2) {
        const auto* bDef = (m_selectedBuilding == 1) ? &simulation::config::HOUSE_1 : &simulation::config::CITY_STORAGE;
        bool isStorage = (m_selectedBuilding == 2);

        m_canPlace = simulation::canPlaceBuilding(registry, chunkManager, m_placementPos, *bDef, isStorage);
    }

    if (currentEnter && !m_prevEnter) {
        if ((m_selectedBuilding == 1 || m_selectedBuilding == 2) && m_canPlace) {
            const auto* bDef = (m_selectedBuilding == 1) ? &simulation::config::HOUSE_1 : &simulation::config::CITY_STORAGE;
            bool isStorage = (m_selectedBuilding == 2);

            simulation::spawnBuilding(registry, chunkManager, *bDef, m_placementPos, isStorage);

            m_canPlace = false;
        }
    }

    if (!registry.ctx().contains<ecs::BuilderModeState>()) {
        registry.ctx().emplace<ecs::BuilderModeState>(ecs::BuilderModeState{m_builderModeActive, m_selectedBuilding, m_canPlace, m_placementPos});
    } else {
        auto& st = registry.ctx().get<ecs::BuilderModeState>();
        st.active = m_builderModeActive;
        st.selectedBuilding = m_selectedBuilding;
        st.canPlace = m_canPlace;
        st.placementPos = m_placementPos;
    }

    m_prevB = currentB; m_prev1 = current1; m_prevEnter = currentEnter;
}

}