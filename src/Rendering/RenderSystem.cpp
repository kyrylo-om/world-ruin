#include "Rendering/RenderSystem.hpp"
#include "Rendering/Renderers/GeometryRenderer.hpp"
#include "Rendering/Renderers/EntityRenderer.hpp"
#include "Rendering/Renderers/FoamRenderer.hpp"
#include "Rendering/Renderers/CanopyRenderer.hpp"
#include "../../include/Rendering/Utils/FastSorter.hpp"
#include "Rendering/Renderers/DebugRenderer.hpp"
#include "Rendering/Renderers/OverlayRenderer.hpp"
#include "Rendering/Renderers/HUDRenderer.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include <algorithm>
#include <iostream>

namespace wr::rendering {

namespace {
    sf::Shader foamShader;
    sf::Clock globalTimeClock;
    bool isShaderLoaded = false;

    const std::string foamVertexShader = R"(
        uniform float u_offset;
        void main() {
            gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
            vec4 texCoords = gl_MultiTexCoord0;
            texCoords.x += u_offset;
            gl_TexCoord[0] = gl_TextureMatrix[0] * texCoords;
            gl_FrontColor = gl_Color;
        }
    )";
}

RenderSystem::RenderSystem(size_t reserveCapacity) : m_showDebug(false) {
    m_layer0.reserve(reserveCapacity);
    m_layer1.reserve(reserveCapacity);
    m_layer2.reserve(reserveCapacity);
    m_layer3.reserve(reserveCapacity);

    if (!m_debugFont.loadFromFile("assets/fonts/Roboto-Bold.ttf")) {
        std::cerr << "[RenderSystem] Warning: Could not load debug font.\n";
    }

    if (!m_gameFont.loadFromFile("assets/fonts/PressStart2P-Regular.ttf")) {
        std::cerr << "[RenderSystem] Warning: Could not load game font.\n";
    }

    if (!isShaderLoaded && sf::Shader::isAvailable()) {
        if (foamShader.loadFromMemory(foamVertexShader, sf::Shader::Vertex)) {
            isShaderLoaded = true;
        }
    }
}

void RenderSystem::updateInterpolation(entt::registry& registry, float dt) noexcept {
    auto alphaView = registry.view<ecs::ScreenPos>();
    for (auto entity : alphaView) {
        auto& sp = alphaView.get<ecs::ScreenPos>(entity);
        if (std::abs(sp.currentAlpha - sp.targetAlpha) > 1.0f) {
            sp.currentAlpha = math::lerp(sp.currentAlpha, sp.targetAlpha, 10.0f * dt);
        } else {
            sp.currentAlpha = sp.targetAlpha;
        }
    }
}

void RenderSystem::render(entt::registry& registry, sf::RenderWindow& window, const Camera& camera, const TileHandler& tiles,
                          const std::unordered_map<math::Vec2i64, std::unique_ptr<world::Chunk>, math::SpatialHash>& chunks,
                          ViewDirection dir, const ActiveDragData& dragData) {

    window.setView(camera.getView());
    float currentAngle = camera.getRotation();
    float currentZoom = camera.getZoom();
    bool useLOD = currentZoom >= 20.0f;
    float timeSeconds = globalTimeClock.getElapsedTime().asSeconds();

    float currentFoamOffset = 0.0f;
    if (isShaderLoaded) {
        int frame = static_cast<int>(timeSeconds * 10.0f) % TileHandler::FOAM_FRAME_COUNT;
        currentFoamOffset = static_cast<float>(frame * TileHandler::FOAM_FRAME_SIZE);
        foamShader.setUniform("u_offset", currentFoamOffset);
    }

    bool animateWind = currentZoom <= 4.0f;

    RenderContext ctx{
        window, camera, tiles, currentAngle,
        static_cast<int>(dir), dir,
        sf::FloatRect(camera.getView().getCenter() - camera.getView().getSize() / 2.0f, camera.getView().getSize()),
        useLOD,
        isShaderLoaded ? &foamShader : nullptr,
        timeSeconds,
        std::sin(timeSeconds * 5.0f) * 0.05f + 0.95f,
        animateWind
    };

    ChunkBounds bounds = camera.getVisibleChunkBounds();
    std::vector<world::Chunk*> sortedChunks;
    sortedChunks.reserve((bounds.maxX - bounds.minX + 1) * (bounds.maxY - bounds.minY + 1));

    for (int64_t cy = bounds.minY; cy <= bounds.maxY; ++cy) {
        for (int64_t cx = bounds.minX; cx <= bounds.maxX; ++cx) {
            auto it = chunks.find({cx, cy});
            if (it != chunks.end() && it->second->state.load(std::memory_order_acquire) == world::ChunkState::Active) {
                sortedChunks.push_back(it->second.get());
            }
        }
    }

    std::sort(sortedChunks.begin(), sortedChunks.end(), [dir](const world::Chunk* a, const world::Chunk* b) {
        if (dir == ViewDirection::North) { if (a->coordinate.y != b->coordinate.y) return a->coordinate.y < b->coordinate.y; return a->coordinate.x < b->coordinate.x; }
        if (dir == ViewDirection::South) { if (a->coordinate.y != b->coordinate.y) return a->coordinate.y > b->coordinate.y; return a->coordinate.x < b->coordinate.x; }
        if (dir == ViewDirection::East)  { if (a->coordinate.x != b->coordinate.x) return a->coordinate.x < b->coordinate.x; return a->coordinate.y < b->coordinate.y; }
        return a->coordinate.x > b->coordinate.x;
    });

    auto getOrbitalOffset = [&](const math::Vec2f& pos) -> math::Vec2f {
        float rad = currentAngle * 3.14159265f / 180.0f;
        return { pos.x * std::cos(rad) - pos.y * std::sin(rad), pos.x * std::sin(rad) + pos.y * std::cos(rad) };
    };

    std::vector<ChunkRenderData> renderData;
    renderData.reserve(sortedChunks.size());

    for (const auto* chunk : sortedChunks) {
        math::Vec2f relPos = camera.getRelativeScreenPosition({chunk->coordinate.x * 64, chunk->coordinate.y * 64});
        math::Vec2f rotPos = getOrbitalOffset(relPos);
        sf::Transform transform;
        transform.translate(rotPos.x, rotPos.y);
        transform.rotate(currentAngle);
        renderData.push_back({chunk, transform});
    }

    if (useLOD) {
        GeometryRenderer::renderLOD(ctx, renderData);
    } else {
        (void)registry.group<ecs::ScreenPos>(entt::get<ecs::SpriteComponent>);

        m_layer0.clear(); m_layer1.clear(); m_layer2.clear(); m_layer3.clear();
        std::vector<entt::entity> visibleTrees;
        visibleTrees.reserve(500);

        sf::FloatRect broadCullBounds = ctx.viewBounds;
        broadCullBounds.left -= 512.0f; broadCullBounds.top -= 512.0f;
        broadCullBounds.width += 1024.0f; broadCullBounds.height += 1024.0f;

        auto addToLayers = [&](entt::entity entity, float zOffset) {
            if (registry.all_of<ecs::TreeTag>(entity)) visibleTrees.push_back(entity);
            if (zOffset < 0.1f)        m_layer0.push_back(entity);
            else if (zOffset < 64.1f)  m_layer1.push_back(entity);
            else if (zOffset < 128.1f) m_layer2.push_back(entity);
            else                       m_layer3.push_back(entity);
        };

        for (const auto* chunk : sortedChunks) {
            for (entt::entity entity : chunk->entities) {
                if (!registry.valid(entity)) continue;
                const auto& sp = registry.get<ecs::ScreenPos>(entity);
                float zOffset = 0.0f;
                if (const auto* wp = registry.try_get<ecs::WorldPos>(entity)) zOffset = wp->wz;

                if (!broadCullBounds.contains(sp.x, sp.y - zOffset)) continue;
                addToLayers(entity, zOffset);
            }
        }

        auto dynamicView = registry.view<ecs::ScreenPos, ecs::SpriteComponent>(entt::exclude<ecs::ResourceTag>);
        for (auto entity : dynamicView) {
            const auto& sp = dynamicView.get<ecs::ScreenPos>(entity);
            float zOffset = 0.0f;
            if (const auto* wp = registry.try_get<ecs::WorldPos>(entity)) zOffset = wp->wz;

            if (!broadCullBounds.contains(sp.x, sp.y - zOffset)) continue;
            addToLayers(entity, zOffset);
        }

        std::vector<entt::entity> pendingL0, pendingL1, pendingL2, pendingL3;
        auto pendingView = registry.view<ecs::PendingTaskArea>();
        for (auto e : pendingView) {
            auto& area = pendingView.get<ecs::PendingTaskArea>(e);
            uint8_t lvl = OverlayRenderer::getElevationInfo(chunks, area.startWorld, area.endWorld).first;
            if (lvl <= 2) pendingL0.push_back(e);
            else if (lvl <= 4) pendingL1.push_back(e);
            else if (lvl <= 6) pendingL2.push_back(e);
            else pendingL3.push_back(e);
        }

        std::vector<entt::entity> taskL0, taskL1, taskL2, taskL3;
        auto taskView = registry.view<ecs::TaskArea>();
        for (auto e : taskView) {
            auto& area = taskView.get<ecs::TaskArea>(e);
            uint8_t lvl = OverlayRenderer::getElevationInfo(chunks, area.startWorld, area.endWorld).first;
            if (lvl <= 2) taskL0.push_back(e);
            else if (lvl <= 4) taskL1.push_back(e);
            else if (lvl <= 6) taskL2.push_back(e);
            else taskL3.push_back(e);
        }

        GeometryRenderer::renderWater(ctx, renderData);
        FoamRenderer::render(ctx, registry, m_layer0);
        GeometryRenderer::renderBaseGround(ctx, renderData);

        OverlayRenderer::renderLayer(ctx, registry, chunks, pendingL0, taskL0, dragData, 2, m_gameFont);

        GeometryRenderer::renderTier1_Faces(ctx, renderData);
        EntityRenderer::renderEntities(ctx, registry, m_layer0, false, m_showDebug);
        GeometryRenderer::renderTier1_Tops(ctx, renderData);

        OverlayRenderer::renderLayer(ctx, registry, chunks, pendingL1, taskL1, dragData, 4, m_gameFont);

        GeometryRenderer::renderTier2_Faces(ctx, renderData);
        EntityRenderer::renderEntities(ctx, registry, m_layer1, false, m_showDebug);
        GeometryRenderer::renderTier2_Tops(ctx, renderData);

        OverlayRenderer::renderLayer(ctx, registry, chunks, pendingL2, taskL2, dragData, 6, m_gameFont);

        GeometryRenderer::renderTier3_Faces(ctx, renderData);
        EntityRenderer::renderEntities(ctx, registry, m_layer2, false, m_showDebug);
        GeometryRenderer::renderTier3_Tops(ctx, renderData);

        OverlayRenderer::renderLayer(ctx, registry, chunks, pendingL3, taskL3, dragData, 8, m_gameFont);

        EntityRenderer::renderEntities(ctx, registry, m_layer3, false, m_showDebug);

        FastSorter::sortByScreenY(visibleTrees, registry);
        CanopyRenderer::render(ctx, registry, visibleTrees);

        OverlayRenderer::renderText(ctx, registry, chunks, m_gameFont);
        HUDRenderer::render(ctx, registry);

        if (m_showDebug) {
            sf::Text debugText;
            debugText.setFont(m_debugFont);
            debugText.setCharacterSize(14);
            debugText.setFillColor(sf::Color::Yellow);
            debugText.setOutlineColor(sf::Color::Black);
            debugText.setOutlineThickness(1.2f);
            DebugRenderer::render(ctx, renderData, debugText);
        }
    }
}

}