#include "../../include/Input/CursorBuilderSystem.hpp"
#include "../../include/Config/BuildingsConfig.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include "Rendering/TileHandler.hpp"
#include <SFML/Window/Keyboard.hpp>
#include <cmath>

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

        float hw = bDef->width / 2.0f;
        float hh = bDef->height / 2.0f;
        float minX = m_placementPos.x - hw;
        float maxX = m_placementPos.x + hw;
        float minY = m_placementPos.y - hh;
        float maxY = m_placementPos.y + hh;

        int minBx = static_cast<int>(std::floor(minX + 0.01f));
        int maxBx = static_cast<int>(std::floor(maxX - 0.01f));
        int minBy = static_cast<int>(std::floor(minY + 0.01f));
        int maxBy = static_cast<int>(std::floor(maxY - 0.01f));

        m_canPlace = true;
        int baseLevel = -1;
        for (int by = minBy; by <= maxBy; ++by) {
            for (int bx = minBx; bx <= maxBx; ++bx) {
                auto info = chunkManager.getGlobalTileInfo(bx, by);
                if (info.type == core::TerrainType::Water || info.isRamp) {
                    m_canPlace = false; break;
                }
                if (baseLevel == -1) baseLevel = info.elevationLevel;
                else if (baseLevel != info.elevationLevel) {
                    m_canPlace = false; break;
                }
            }
            if (!m_canPlace) break;
        }

        if (m_canPlace) {
            auto resView = registry.view<ecs::WorldPos>();
            for (auto e : resView) {
                bool isObstacle = registry.any_of<ecs::TreeTag, ecs::RockTag, ecs::SmallRockTag, ecs::BushTag, ecs::BuildingTag>(e);
                if (!isObstacle) continue;

                const auto& wp = resView.get<ecs::WorldPos>(e);
                float r = 0.4f;
                if (auto* hb = registry.try_get<ecs::Hitbox>(e)) r = hb->radius / 64.0f;
                else if (auto* ca = registry.try_get<ecs::ClickArea>(e)) r = ca->radius / 64.0f;

                if (wp.wx + r > minX && wp.wx - r < maxX && wp.wy + r > minY && wp.wy - r < maxY) {
                    m_canPlace = false; break;
                }
            }
        }

        if (m_canPlace) {
            float resW = bDef->resourceZoneWidth / 2.0f;
            float resH = bDef->resourceZoneHeight / 2.0f;
            float resMinX = m_placementPos.x - resW;
            float resMaxX = m_placementPos.x + resW;
            float resMinY = m_placementPos.y - resH;
            float resMaxY = m_placementPos.y + resH;

            if (m_selectedBuilding == 1) {
                auto storeView = registry.view<ecs::CityStorageTag, ecs::WorldPos>();
                for (auto e : storeView) {
                    auto& sWp = storeView.get<ecs::WorldPos>(e);
                    float sRw = simulation::config::CITY_STORAGE.resourceZoneWidth / 2.0f;
                    float sRh = simulation::config::CITY_STORAGE.resourceZoneHeight / 2.0f;
                    if (!(resMaxX < sWp.wx - sRw || resMinX > sWp.wx + sRw || resMaxY < sWp.wy - sRh || resMinY > sWp.wy + sRh)) {
                        m_canPlace = false; break;
                    }
                }
            } else if (m_selectedBuilding == 2) {
                auto houseView = registry.view<ecs::BuildingTag, ecs::WorldPos>(entt::exclude<ecs::CityStorageTag>);
                for (auto e : houseView) {
                    auto& hWp = houseView.get<ecs::WorldPos>(e);
                    float hRw = simulation::config::HOUSE_1.resourceZoneWidth / 2.0f;
                    float hRh = simulation::config::HOUSE_1.resourceZoneHeight / 2.0f;
                    if (!(resMaxX < hWp.wx - hRw || resMinX > hWp.wx + hRw || resMaxY < hWp.wy - hRh || resMinY > hWp.wy + hRh)) {
                        m_canPlace = false; break;
                    }
                }
            }
        }
    }

    if (currentEnter && !m_prevEnter) {
        if ((m_selectedBuilding == 1 || m_selectedBuilding == 2) && m_canPlace) {
            const auto* bDef = (m_selectedBuilding == 1) ? &simulation::config::HOUSE_1 : &simulation::config::CITY_STORAGE;

            auto bEnt = registry.create();
            registry.emplace<ecs::BuildingTag>(bEnt);

            int bX = static_cast<int>(std::floor(m_placementPos.x));
            int bY = static_cast<int>(std::floor(m_placementPos.y));
            uint8_t level = chunkManager.getGlobalTileInfo(bX, bY).elevationLevel;
            float zHeight = (level - 1) * 32.0f;

            registry.emplace<ecs::LogicalPos>(bEnt, bX, bY);
            registry.emplace<ecs::WorldPos>(bEnt, m_placementPos.x, m_placementPos.y, zHeight, zHeight);
            registry.emplace<ecs::ScreenPos>(bEnt, 0.f, 0.f, 255.0f, 255.0f, static_cast<uint8_t>(0));

            registry.emplace<ecs::Hitbox>(bEnt, (bDef->hitboxWidth * 64.0f) / 2.0f);
            registry.emplace<ecs::ClickArea>(bEnt, (bDef->hitboxWidth * 64.0f) / 2.0f);

            bool isInstantlyBuilt = (m_selectedBuilding == 2);

            registry.emplace<ecs::ConstructionData>(bEnt,
                bDef->cost.wood, bDef->cost.rock,
                bDef->cost.wood, bDef->cost.rock,
                bDef->resourceZoneWidth, bDef->resourceZoneHeight,
                isInstantlyBuilt, (isInstantlyBuilt ? bDef->buildTime : 0.0f), bDef->buildTime
            );

            if (m_selectedBuilding == 1) {
                auto& spr = registry.emplace<ecs::SpriteComponent>(bEnt).sprite;
                auto* tilesPtr = registry.ctx().find<const rendering::TileHandler*>();
                if (tilesPtr) {
                    spr.setTexture((*tilesPtr)->getHouse1Texture());
                    auto bounds = spr.getLocalBounds();
                    spr.setOrigin(bounds.width / 2.0f, bounds.height * 0.65f);
                    spr.setScale(1.8f, 1.8f);
                }
                registry.emplace<ecs::SolidTag>(bEnt);
            } else {
                registry.emplace<ecs::CityStorageTag>(bEnt);
            }

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