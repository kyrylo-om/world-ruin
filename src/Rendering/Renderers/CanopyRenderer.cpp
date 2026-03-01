#include "Rendering/Renderers/CanopyRenderer.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <cmath>

namespace wr::rendering {

void CanopyRenderer::render(const RenderContext& ctx, entt::registry& registry, const std::vector<entt::entity>& sortedTrees) {
    sf::FloatRect cullBounds = ctx.viewBounds;
    cullBounds.left -= 256.0f; cullBounds.top -= 256.0f;
    cullBounds.width += 512.0f; cullBounds.height += 512.0f;

    const sf::Texture* currentBatchTexture = nullptr;
    static std::vector<sf::Vertex> batch;
    if (batch.capacity() < 150000) batch.reserve(150000);
    batch.clear();

    auto flushBatch = [&]() {
        if (!batch.empty() && currentBatchTexture) {
            sf::RenderStates states;
            states.texture = currentBatchTexture;
            ctx.window.draw(batch.data(), batch.size(), sf::Quads, states);
            batch.clear();
        }
    };

    for (auto e : sortedTrees) {
        const auto& sp = registry.get<ecs::ScreenPos>(e);
        if (sp.currentAlpha < 1.0f) continue;

        const auto& wp = registry.get<ecs::WorldPos>(e);
        float zOffset = wp.wz;
        float worldX = wp.wx;
        float worldY = wp.wy;

        if (!cullBounds.contains(sp.x, sp.y - zOffset)) continue;

        bool isShaking = false;
        float visualShakeX = 0.0f;
        if (const auto* resData = registry.try_get<ecs::ResourceData>(e)) {
            if (resData->shakeTimer > 0.0f) {
                isShaking = true;
                visualShakeX = std::sin(resData->shakeTimer * 60.0f) * 4.0f;
            }
        }

        bool isMarked = false;
        sf::Color glowColor = sf::Color::White;

        if (const auto* marked = registry.try_get<ecs::MarkedForHarvestTag>(e)) {
            isMarked = true;
            glowColor = marked->color;
        } else if (const auto* preview = registry.try_get<ecs::PreviewHarvestTag>(e)) {
            isMarked = true;
            glowColor = preview->color;
        }

        if (!isShaking && !isMarked) {
            if (auto* canopyQuad = registry.try_get<ecs::CachedCanopyQuad>(e)) {
                if (canopyQuad->texture != currentBatchTexture) {
                    flushBatch();
                    currentBatchTexture = canopyQuad->texture;
                }

                sf::Uint8 a = static_cast<sf::Uint8>(sp.currentAlpha);
                float px = sp.x;
                float py = sp.y - zOffset;

                float shiftX = 0.0f;
                if (ctx.animateWind) {
                    float pseudoRandom = std::fmod(std::abs(worldX * 12.9898f + worldY * 78.233f), 1.0f);
                    float pauseDuration = 0.2f + (pseudoRandom * 0.6f);
                    float animDuration = 8.0f / 7.0f;
                    float totalCycle = animDuration + pauseDuration;

                    float phaseOffset = pseudoRandom * totalCycle;
                    float localTime = ctx.timeSeconds + (worldX * 0.008f) + (worldY * 0.012f) + phaseOffset;
                    float cycleTime = std::fmod(localTime, totalCycle);
                    if (cycleTime < 0.0f) cycleTime += totalCycle;

                    int frame = (cycleTime < animDuration) ? static_cast<int>(cycleTime * 7.0f) : 0;
                    shiftX = std::min(frame, 7) * 192.0f;
                }

                for(int i=0; i<4; ++i) {
                    sf::Vertex v = canopyQuad->localVertices[i];
                    v.position.x += px; v.position.y += py;
                    v.color.a = a;
                    v.texCoords.x += shiftX;
                    batch.push_back(v);
                }
            }
        } else {
            flushBatch();

            if (auto* canopyQuad = registry.try_get<ecs::CachedCanopyQuad>(e)) {
                sf::VertexArray singleQuad(sf::Quads, 4);
                float px = sp.x + visualShakeX;
                float py = sp.y - zOffset;

                float shiftX = 0.0f;
                if (ctx.animateWind) {
                    float pseudoRandom = std::fmod(std::abs(worldX * 12.9898f + worldY * 78.233f), 1.0f);
                    float pauseDuration = 0.2f + (pseudoRandom * 0.6f);
                    float animDuration = 8.0f / 7.0f;
                    float totalCycle = animDuration + pauseDuration;

                    float phaseOffset = pseudoRandom * totalCycle;
                    float localTime = ctx.timeSeconds + (worldX * 0.008f) + (worldY * 0.012f) + phaseOffset;
                    float cycleTime = std::fmod(localTime, totalCycle);
                    if (cycleTime < 0.0f) cycleTime += totalCycle;

                    if (cycleTime < animDuration) {
                        int frame = static_cast<int>(cycleTime * 7.0f);
                        shiftX = std::min(frame, 7) * 192.0f;
                    }
                }

                sf::Uint8 baseAlpha = static_cast<sf::Uint8>(sp.currentAlpha);
                for(int i=0; i<4; ++i) {
                    singleQuad[i] = canopyQuad->localVertices[i];
                    singleQuad[i].position.x += px;
                    singleQuad[i].position.y += py;
                    singleQuad[i].texCoords.x += shiftX;
                    singleQuad[i].color.a = baseAlpha;
                }

                sf::RenderStates normalStates;
                normalStates.texture = canopyQuad->texture;
                ctx.window.draw(singleQuad, normalStates);

                if (isMarked) {
                    float pulse = (std::sin(ctx.timeSeconds * 6.0f) + 1.0f) * 0.5f;
                    sf::Uint8 glowAlpha = static_cast<sf::Uint8>(pulse * 140.0f);
                    for(int i=0; i<4; ++i) {
                        singleQuad[i].color = sf::Color(glowColor.r, glowColor.g, glowColor.b, glowAlpha);
                    }
                    sf::RenderStates glowStates;
                    glowStates.texture = canopyQuad->texture;
                    glowStates.blendMode = sf::BlendAdd;
                    ctx.window.draw(singleQuad, glowStates);
                }
            }
        }
    }
    flushBatch();
}

}