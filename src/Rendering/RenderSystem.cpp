#include "Rendering/RenderSystem.hpp"
#include "Rendering/Renderers/GeometryRenderer.hpp"
#include "Rendering/Renderers/EntityRenderer.hpp"
#include "Rendering/Renderers/FoamRenderer.hpp"
#include "Rendering/Renderers/CanopyRenderer.hpp"
#include "../../include/Rendering/Utils/FastSorter.hpp"
#include "Rendering/Renderers/DebugRenderer.hpp"
#include "Rendering/Renderers/OverlayRenderer.hpp"
#include "Rendering/Renderers/HUDRenderer.hpp"
#include "Core/Profiler.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include <algorithm>
#include <iostream>
#include <string>
#include <stdio.h>

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

    m_cachedChunkLayer0.reserve(reserveCapacity);
    m_cachedChunkLayer1.reserve(reserveCapacity);
    m_cachedChunkLayer2.reserve(reserveCapacity);
    m_cachedChunkLayer3.reserve(reserveCapacity);
    m_cachedChunkVisibleTrees.reserve(500);

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
    core::ScopedTimer interpTimer("4.0_Render_Interpolation");
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

    std::vector<ChunkRenderData> renderData;
    std::vector<world::Chunk*> sortedChunks;

    {
        core::ScopedTimer prepTimer("4.1_Render_Prep_Chunks");
        ChunkBounds bounds = camera.getVisibleChunkBounds();
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

        renderData.reserve(sortedChunks.size());

        for (const auto* chunk : sortedChunks) {
            math::Vec2f relPos = camera.getRelativeScreenPosition({chunk->coordinate.x * 64, chunk->coordinate.y * 64});
            math::Vec2f rotPos = getOrbitalOffset(relPos);
            sf::Transform transform;
            transform.translate(rotPos.x, rotPos.y);
            transform.rotate(currentAngle);
            renderData.push_back({chunk, transform});
        }
    }

    if (useLOD) {
        GeometryRenderer::renderLOD(ctx, renderData);
    } else {
        std::vector<entt::entity> visibleTrees;
        std::vector<entt::entity> pendingL0, pendingL1, pendingL2, pendingL3;
        std::vector<entt::entity> taskL0, taskL1, taskL2, taskL3;

        {
            core::ScopedTimer cullTimer("4.2_Render_Cull_Entities");
            (void)registry.group<ecs::ScreenPos>(entt::get<ecs::SpriteComponent>);

            sf::FloatRect broadCullBounds = ctx.viewBounds;
            broadCullBounds.left -= 512.0f; broadCullBounds.top -= 512.0f;
            broadCullBounds.width += 1024.0f; broadCullBounds.height += 1024.0f;

            float cullCenterX = broadCullBounds.left + (broadCullBounds.width * 0.5f);
            float cullCenterY = broadCullBounds.top + (broadCullBounds.height * 0.5f);

            auto addToLayers = [&](entt::entity entity, float zOffset) {
                if (registry.all_of<ecs::TreeTag>(entity)) visibleTrees.push_back(entity);
                if (zOffset < 0.1f)        m_layer0.push_back(entity);
                else if (zOffset < 64.1f)  m_layer1.push_back(entity);
                else if (zOffset < 128.1f) m_layer2.push_back(entity);
                else                       m_layer3.push_back(entity);
            };

            std::vector<ChunkCullFingerprint> currentFingerprint;
            currentFingerprint.reserve(sortedChunks.size());
            size_t estimatedChunkEntityCount = 0;
            for (const auto* chunk : sortedChunks) {
                currentFingerprint.push_back({chunk->coordinate, chunk->entities.size()});
                estimatedChunkEntityCount += chunk->entities.size();
            }

            bool fingerprintChanged = !m_chunkCullCacheValid || currentFingerprint.size() != m_chunkCullFingerprint.size();

            if (!fingerprintChanged) {
                for (size_t i = 0; i < currentFingerprint.size(); ++i) {
                    const auto& a = currentFingerprint[i];
                    const auto& b = m_chunkCullFingerprint[i];
                    if (a.coord.x != b.coord.x || a.coord.y != b.coord.y || a.entityCount != b.entityCount) {
                        fingerprintChanged = true;
                        break;
                    }
                }
            }

            bool projectionChanged = !m_chunkCullCacheValid ||
                                     std::abs(currentAngle - m_lastCullAngle) > 0.001f ||
                                     std::abs(currentZoom - m_lastCullZoom) > 0.001f;

            bool cullWindowShifted = !m_hasLastCullCenter ||
                                     std::abs(cullCenterX - m_lastCullCenterX) > 256.0f ||
                                     std::abs(cullCenterY - m_lastCullCenterY) > 256.0f;

            if (fingerprintChanged || projectionChanged || cullWindowShifted) {
                m_cachedChunkLayer0.clear();
                m_cachedChunkLayer1.clear();
                m_cachedChunkLayer2.clear();
                m_cachedChunkLayer3.clear();
                m_cachedChunkVisibleTrees.clear();

                m_cachedChunkLayer0.reserve(estimatedChunkEntityCount);
                m_cachedChunkLayer1.reserve(estimatedChunkEntityCount);
                m_cachedChunkLayer2.reserve(estimatedChunkEntityCount);
                m_cachedChunkLayer3.reserve(estimatedChunkEntityCount);

                auto addToCachedLayers = [&](entt::entity entity, float zOffset) {
                    if (registry.all_of<ecs::TreeTag>(entity)) m_cachedChunkVisibleTrees.push_back(entity);
                    if (zOffset < 0.1f)        m_cachedChunkLayer0.push_back(entity);
                    else if (zOffset < 64.1f)  m_cachedChunkLayer1.push_back(entity);
                    else if (zOffset < 128.1f) m_cachedChunkLayer2.push_back(entity);
                    else                       m_cachedChunkLayer3.push_back(entity);
                };

                for (const auto* chunk : sortedChunks) {
                    for (entt::entity entity : chunk->entities) {
                        if (!registry.valid(entity)) continue;
                        if (!registry.all_of<ecs::ScreenPos, ecs::SpriteComponent>(entity)) continue;
                        if (!registry.all_of<ecs::ResourceTag>(entity)) continue;
                        const auto& sp = registry.get<ecs::ScreenPos>(entity);
                        float zOffset = 0.0f;
                        if (const auto* wp = registry.try_get<ecs::WorldPos>(entity)) zOffset = wp->wz;

                        if (!broadCullBounds.contains(sp.x, sp.y - zOffset)) continue;
                        addToCachedLayers(entity, zOffset);
                    }
                }

                FastSorter::sortByScreenY(m_cachedChunkLayer0, registry);
                FastSorter::sortByScreenY(m_cachedChunkLayer1, registry);
                FastSorter::sortByScreenY(m_cachedChunkLayer2, registry);
                FastSorter::sortByScreenY(m_cachedChunkLayer3, registry);
                FastSorter::sortByScreenY(m_cachedChunkVisibleTrees, registry);

                m_chunkCullFingerprint = std::move(currentFingerprint);
                m_lastCullAngle = currentAngle;
                m_lastCullZoom = currentZoom;
                m_lastCullCenterX = cullCenterX;
                m_lastCullCenterY = cullCenterY;
                m_hasLastCullCenter = true;
                m_chunkCullCacheValid = true;
            }

            m_layer0 = m_cachedChunkLayer0;
            m_layer1 = m_cachedChunkLayer1;
            m_layer2 = m_cachedChunkLayer2;
            m_layer3 = m_cachedChunkLayer3;
            visibleTrees = m_cachedChunkVisibleTrees;

            auto sanitizeLayer = [&](std::vector<entt::entity>& layer) {
                layer.erase(std::remove_if(layer.begin(), layer.end(), [&](entt::entity e) {
                    return !registry.valid(e) || !registry.all_of<ecs::ScreenPos, ecs::SpriteComponent>(e);
                }), layer.end());
            };

            sanitizeLayer(m_layer0);
            sanitizeLayer(m_layer1);
            sanitizeLayer(m_layer2);
            sanitizeLayer(m_layer3);

            visibleTrees.erase(std::remove_if(visibleTrees.begin(), visibleTrees.end(), [&](entt::entity e) {
                return !registry.valid(e) || !registry.all_of<ecs::TreeTag, ecs::ScreenPos, ecs::WorldPos>(e);
            }), visibleTrees.end());

            const size_t staticLayer0Count = m_layer0.size();
            const size_t staticLayer1Count = m_layer1.size();
            const size_t staticLayer2Count = m_layer2.size();
            const size_t staticLayer3Count = m_layer3.size();

            auto dynamicView = registry.view<ecs::ScreenPos, ecs::SpriteComponent>(entt::exclude<ecs::ResourceTag>);
            for (auto entity : dynamicView) {
                const auto& sp = dynamicView.get<ecs::ScreenPos>(entity);
                float zOffset = 0.0f;
                if (const auto* wp = registry.try_get<ecs::WorldPos>(entity)) zOffset = wp->wz;

                if (!broadCullBounds.contains(sp.x, sp.y - zOffset)) continue;
                addToLayers(entity, zOffset);
            }

            auto screenYLess = [&](entt::entity a, entt::entity b) {
                return registry.get<ecs::ScreenPos>(a).y < registry.get<ecs::ScreenPos>(b).y;
            };

            auto mergeDynamicIntoLayer = [&](std::vector<entt::entity>& layer, size_t staticCount) {
                if (layer.size() <= staticCount) return;
                auto mid = layer.begin() + staticCount;
                std::sort(mid, layer.end(), screenYLess);
                std::inplace_merge(layer.begin(), mid, layer.end(), screenYLess);
            };

            mergeDynamicIntoLayer(m_layer0, staticLayer0Count);
            mergeDynamicIntoLayer(m_layer1, staticLayer1Count);
            mergeDynamicIntoLayer(m_layer2, staticLayer2Count);
            mergeDynamicIntoLayer(m_layer3, staticLayer3Count);

            auto pendingView = registry.view<ecs::PendingTaskArea>();
            for (auto e : pendingView) {
                auto& area = pendingView.get<ecs::PendingTaskArea>(e);
                if (area.areas.empty()) continue;
                uint8_t lvl = OverlayRenderer::getElevationInfo(chunks, area.areas.front().startWorld, area.areas.front().endWorld).first;
                if (lvl <= 2) pendingL0.push_back(e);
                else if (lvl <= 4) pendingL1.push_back(e);
                else if (lvl <= 6) pendingL2.push_back(e);
                else pendingL3.push_back(e);
            }

            auto taskView = registry.view<ecs::TaskArea>();
            for (auto e : taskView) {
                auto& area = taskView.get<ecs::TaskArea>(e);
                if (area.areas.empty()) continue;
                uint8_t lvl = OverlayRenderer::getElevationInfo(chunks, area.areas.front().startWorld, area.areas.front().endWorld).first;
                if (lvl <= 2) taskL0.push_back(e);
                else if (lvl <= 4) taskL1.push_back(e);
                else if (lvl <= 6) taskL2.push_back(e);
                else taskL3.push_back(e);
            }
        }

        {
            core::ScopedTimer drawGeomTimer("4.3_Render_Geom");
            GeometryRenderer::renderWater(ctx, renderData);
            FoamRenderer::render(ctx, registry, m_layer0);
            GeometryRenderer::renderBaseGround(ctx, renderData);
        }

        {
            core::ScopedTimer drawEntTimer("4.4_Render_Entities");
            OverlayRenderer::renderLayer(ctx, registry, chunks, pendingL0, taskL0, dragData, 2, m_gameFont);
            GeometryRenderer::renderTier1_Faces(ctx, renderData);
            EntityRenderer::renderEntities(ctx, registry, m_layer0, true, m_showDebug);
            GeometryRenderer::renderTier1_Tops(ctx, renderData);

            OverlayRenderer::renderLayer(ctx, registry, chunks, pendingL1, taskL1, dragData, 4, m_gameFont);
            GeometryRenderer::renderTier2_Faces(ctx, renderData);
            EntityRenderer::renderEntities(ctx, registry, m_layer1, true, m_showDebug);
            GeometryRenderer::renderTier2_Tops(ctx, renderData);

            OverlayRenderer::renderLayer(ctx, registry, chunks, pendingL2, taskL2, dragData, 6, m_gameFont);
            GeometryRenderer::renderTier3_Faces(ctx, renderData);
            EntityRenderer::renderEntities(ctx, registry, m_layer2, true, m_showDebug);
            GeometryRenderer::renderTier3_Tops(ctx, renderData);

            OverlayRenderer::renderLayer(ctx, registry, chunks, pendingL3, taskL3, dragData, 8, m_gameFont);
            EntityRenderer::renderEntities(ctx, registry, m_layer3, true, m_showDebug);

            CanopyRenderer::render(ctx, registry, visibleTrees);
        }

        {
            core::ScopedTimer hudTimer("4.5_Render_HUD");
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

    static sf::Clock fpsClock;
    static int frameCount = 0;
    static int currentFPS = 0;

    frameCount++;
    if (fpsClock.getElapsedTime().asSeconds() >= 1.0f) {
        currentFPS = frameCount;
        frameCount = 0;
        fpsClock.restart();
    }

    sf::View oldView = window.getView();
    sf::View defaultView = window.getDefaultView();
    window.setView(defaultView);

    sf::Text fpsText("FPS: " + std::to_string(currentFPS), m_gameFont, 8);
    fpsText.setFillColor(sf::Color::Green);
    fpsText.setOutlineColor(sf::Color::Black);
    fpsText.setOutlineThickness(2.0f);
    fpsText.setPosition(15.0f, defaultView.getSize().y - 35.0f);
    window.draw(fpsText);

    if (registry.ctx().get<ecs::GameMode>() == ecs::GameMode::Simulation) {
        float simTime = globalTimeClock.getElapsedTime().asSeconds();
        int minutes = static_cast<int>(simTime) / 60;
        int seconds = static_cast<int>(simTime) % 60;

        char timeStr[64];
        snprintf(timeStr, sizeof(timeStr), "SIMULATION time: %02d:%02d", minutes, seconds);

        sf::Text simText(timeStr, m_gameFont, 10);
        simText.setFillColor(sf::Color(100, 255, 100));
        simText.setOutlineColor(sf::Color::Black);
        simText.setOutlineThickness(2.0f);
        sf::FloatRect bounds = simText.getLocalBounds();
        simText.setPosition(defaultView.getSize().x / 2.0f - bounds.width / 2.0f, defaultView.getSize().y - 25.0f);
        window.draw(simText);
    }

    window.setView(oldView);
}

}