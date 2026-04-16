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
        struct ChunkCullFingerprint {
            math::Vec2i64 coord;
            size_t entityCount;
        };

        std::vector<entt::entity> m_layer0;
        std::vector<entt::entity> m_layer1;
        std::vector<entt::entity> m_layer2;
        std::vector<entt::entity> m_layer3;

        std::vector<ChunkCullFingerprint> m_chunkCullFingerprint;
        std::vector<entt::entity> m_cachedChunkLayer0;
        std::vector<entt::entity> m_cachedChunkLayer1;
        std::vector<entt::entity> m_cachedChunkLayer2;
        std::vector<entt::entity> m_cachedChunkLayer3;
        std::vector<entt::entity> m_cachedChunkVisibleTrees;

        bool m_chunkCullCacheValid{false};
        float m_lastCullAngle{0.0f};
        float m_lastCullZoom{0.0f};
        float m_lastCullCenterX{0.0f};
        float m_lastCullCenterY{0.0f};
        bool m_hasLastCullCenter{false};

        sf::Font m_debugFont;
        sf::Font m_gameFont;
        bool m_showDebug;
    };

}