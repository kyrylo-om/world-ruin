#pragma once

#include "Camera.hpp"
#include "TileHandler.hpp"
#include "World/Chunk.hpp"
#include "Perspective.hpp"
#include "Rendering/Renderers/OverlayRenderer.hpp"
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Text.hpp>
#include <entt/entt.hpp>
#include <vector>
#include <unordered_map>
#include <memory>

namespace wr::rendering {

    class RenderSystem {
    public:
        explicit RenderSystem(size_t reserveCapacity = 15000);

        void updateInterpolation(entt::registry& registry, float dt) noexcept;

        void render(entt::registry& registry, sf::RenderWindow& window, const Camera& camera, const TileHandler& tiles,
                    const std::unordered_map<math::Vec2i64, std::unique_ptr<world::Chunk>, math::SpatialHash>& chunks,
                    ViewDirection dir, const ActiveDragData& dragData);

        void toggleDebug() noexcept { m_showDebug = !m_showDebug; }

    private:
        std::vector<entt::entity> m_layer0;
        std::vector<entt::entity> m_layer1;
        std::vector<entt::entity> m_layer2;
        std::vector<entt::entity> m_layer3;

        sf::Font m_debugFont;
        sf::Font m_gameFont;
        bool m_showDebug;
    };

}