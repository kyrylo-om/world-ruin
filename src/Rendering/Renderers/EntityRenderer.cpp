#include "Rendering/Renderers/EntityRenderer.hpp"
#include "Rendering/Renderers/EntityDebugRenderer.hpp"
#include "../../../include/Rendering/Utils/FastSorter.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <algorithm>
#include <cmath>

namespace wr::rendering {

void EntityRenderer::renderEntities(const RenderContext& ctx, entt::registry& registry, std::vector<entt::entity>& entities, bool skipSort, bool showDebug) {
    if (!skipSort) {
        FastSorter::sortByScreenY(entities, registry);
    }

    sf::FloatRect cullBounds = ctx.viewBounds;
    cullBounds.left -= 256.0f; cullBounds.top -= 256.0f;
    cullBounds.width += 512.0f; cullBounds.height += 512.0f;

    const sf::Texture* currentBatchTexture = nullptr;

    static std::vector<sf::Vertex> opaqueBatch;
    static std::vector<sf::Vertex> glowBatch;
    if (opaqueBatch.capacity() < 150000) opaqueBatch.reserve(150000);
    if (glowBatch.capacity() < 150000) glowBatch.reserve(150000);
    opaqueBatch.clear();
    glowBatch.clear();

    auto flushBatches = [&]() {
        if (!opaqueBatch.empty() && currentBatchTexture) {
            sf::RenderStates states;
            states.texture = currentBatchTexture;
            ctx.window.draw(opaqueBatch.data(), opaqueBatch.size(), sf::Quads, states);
            opaqueBatch.clear();
        }
        if (!glowBatch.empty() && currentBatchTexture) {
            sf::RenderStates glowStates;
            glowStates.texture = currentBatchTexture;
            glowStates.blendMode = sf::BlendAdd;
            ctx.window.draw(glowBatch.data(), glowBatch.size(), sf::Quads, glowStates);
            glowBatch.clear();
        }
    };

    for (entt::entity e : entities) {
        const auto& sp = registry.get<ecs::ScreenPos>(e);
        auto& spr = registry.get<ecs::SpriteComponent>(e).sprite;

        float zOffset = 0.0f;
        float jumpOffset = 0.0f;
        bool isOnWater = false;
        float worldX = 0.0f;
        float worldY = 0.0f;

        if (const auto* wp = registry.try_get<ecs::WorldPos>(e)) {
            zOffset = wp->wz;
            jumpOffset = wp->zJump;
            isOnWater = wp->isOnWater;
            worldX = wp->wx;
            worldY = wp->wy;
        }

        if (sp.currentAlpha < 1.0f) continue;

        bool isRock = registry.all_of<ecs::RockTag>(e);
        bool isSmallRock = registry.all_of<ecs::SmallRockTag>(e);
        bool isTree = registry.all_of<ecs::TreeTag>(e);
        bool isBush = registry.all_of<ecs::BushTag>(e);
        bool isLog  = registry.all_of<ecs::LogTag>(e);
        bool isDebris = registry.all_of<ecs::DebrisTag>(e);
        bool isBuilding = registry.all_of<ecs::BuildingTag>(e);

        if (!isDebris && !cullBounds.contains(sp.x, sp.y - zOffset)) {
            continue;
        }

        bool isShaking = false;
        float visualShakeX = 0.0f;
        float visualShakeY = 0.0f;

        if (const auto* resData = registry.try_get<ecs::ResourceData>(e)) {
            if (resData->shakeTimer > 0.0f) {
                isShaking = true;
                visualShakeX = std::sin(resData->shakeTimer * 60.0f) * 4.0f;
                if (isRock || isSmallRock) visualShakeY = std::cos(resData->shakeTimer * 50.0f) * 3.0f;
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

        bool isResource = (isRock || isSmallRock || isTree || isBush || isLog);
        bool canBatch = isResource && !isShaking && !isBuilding;

        if (canBatch) {
            if (auto* quad = registry.try_get<ecs::CachedQuad>(e)) {
                if (quad->texture != currentBatchTexture) {
                    flushBatches();
                    currentBatchTexture = quad->texture;
                }

                sf::Uint8 a = static_cast<sf::Uint8>(sp.currentAlpha);
                float px = sp.x;
                float py = sp.y - zOffset;

                float shiftX = 0.0f;
                if (isTree || isBush) {
                    if (ctx.animateWind) {
                        float pseudoRandom = std::fmod(std::abs(worldX * 12.9898f + worldY * 78.233f), 1.0f);
                        float pauseDuration = 0.2f + (pseudoRandom * 0.6f);
                        float animSpeed = 7.0f;
                        float animDuration = 8.0f / animSpeed;
                        float totalCycle = animDuration + pauseDuration;

                        float phaseOffset = pseudoRandom * totalCycle;
                        float timeOffset = (worldX * 0.008f) + (worldY * 0.012f);
                        float localTime = ctx.timeSeconds + timeOffset + phaseOffset;

                        float cycleTime = std::fmod(localTime, totalCycle);
                        if (cycleTime < 0.0f) cycleTime += totalCycle;

                        int frame = 0;
                        if (cycleTime < animDuration) {
                            frame = static_cast<int>(cycleTime * animSpeed);
                            if (frame > 7) frame = 7;
                        }
                        shiftX = frame * (isTree ? 192.0f : 128.0f);
                    }
                }

                sf::Vertex v0 = quad->localVertices[0]; v0.position.x += px; v0.position.y += py; v0.color.a = a; v0.texCoords.x += shiftX;
                sf::Vertex v1 = quad->localVertices[1]; v1.position.x += px; v1.position.y += py; v1.color.a = a; v1.texCoords.x += shiftX;
                sf::Vertex v2 = quad->localVertices[2]; v2.position.x += px; v2.position.y += py; v2.color.a = a; v2.texCoords.x += shiftX;
                sf::Vertex v3 = quad->localVertices[3]; v3.position.x += px; v3.position.y += py; v3.color.a = a; v3.texCoords.x += shiftX;

                opaqueBatch.push_back(v0);
                opaqueBatch.push_back(v1);
                opaqueBatch.push_back(v2);
                opaqueBatch.push_back(v3);

                if (isMarked) {
                    float pulse = (std::sin(ctx.timeSeconds * 6.0f) + 1.0f) * 0.5f;
                    sf::Uint8 glowAlpha = static_cast<sf::Uint8>(pulse * 140.0f);
                    v0.color = sf::Color(glowColor.r, glowColor.g, glowColor.b, glowAlpha);
                    v1.color = sf::Color(glowColor.r, glowColor.g, glowColor.b, glowAlpha);
                    v2.color = sf::Color(glowColor.r, glowColor.g, glowColor.b, glowAlpha);
                    v3.color = sf::Color(glowColor.r, glowColor.g, glowColor.b, glowAlpha);

                    glowBatch.push_back(v0);
                    glowBatch.push_back(v1);
                    glowBatch.push_back(v2);
                    glowBatch.push_back(v3);
                }
            }
        } else {
            flushBatches();

            if (isResource) {
                if (auto* quad = registry.try_get<ecs::CachedQuad>(e)) {
                    sf::VertexArray singleQuad(sf::Quads, 4);
                    float px = sp.x + visualShakeX;
                    float py = sp.y - zOffset + visualShakeY;

                    float shiftX = 0.0f;
                    if (isTree || isBush) {
                        if (ctx.animateWind) {
                            float pseudoRandom = std::fmod(std::abs(worldX * 12.9898f + worldY * 78.233f), 1.0f);
                            float pauseDuration = 0.2f + (pseudoRandom * 0.6f);
                            float animDuration = 8.0f / 7.0f;
                            float totalCycle = animDuration + pauseDuration;

                            float phaseOffset = pseudoRandom * totalCycle;
                            float timeOffset = (worldX * 0.008f) + (worldY * 0.012f);
                            float localTime = ctx.timeSeconds + timeOffset + phaseOffset;

                            float cycleTime = std::fmod(localTime, totalCycle);
                            if (cycleTime < 0.0f) cycleTime += totalCycle;

                            if (cycleTime < animDuration) {
                                int frame = static_cast<int>(cycleTime * 7.0f);
                                shiftX = std::min(frame, 7) * (isTree ? 192.0f : 128.0f);
                            }
                        }
                    }

                    sf::Uint8 baseAlpha = static_cast<sf::Uint8>(sp.currentAlpha);
                    for(int i=0; i<4; ++i) {
                        singleQuad[i] = quad->localVertices[i];
                        singleQuad[i].position.x += px;
                        singleQuad[i].position.y += py;
                        singleQuad[i].texCoords.x += shiftX;
                        singleQuad[i].color.a = baseAlpha;
                    }

                    sf::RenderStates normalStates;
                    normalStates.texture = quad->texture;
                    ctx.window.draw(singleQuad, normalStates);

                    if (isMarked) {
                        float pulse = (std::sin(ctx.timeSeconds * 6.0f) + 1.0f) * 0.5f;
                        sf::Uint8 glowAlpha = static_cast<sf::Uint8>(pulse * 140.0f);
                        for(int i=0; i<4; ++i) {
                            singleQuad[i].color = sf::Color(glowColor.r, glowColor.g, glowColor.b, glowAlpha);
                        }
                        sf::RenderStates glowStates;
                        glowStates.texture = quad->texture;
                        glowStates.blendMode = sf::BlendAdd;
                        ctx.window.draw(singleQuad, glowStates);
                    }
                }
            } else if (isDebris) {
                auto& debris = registry.get<ecs::DebrisData>(e);
                sf::FloatRect bounds = spr.getLocalBounds();
                spr.setOrigin(bounds.width / 2.0f, bounds.height / 2.0f);

                float lifeRatio = std::clamp(debris.lifeTimer / debris.maxLife, 0.0f, 1.0f);
                float timeActive = debris.maxLife - debris.lifeTimer;

                if (debris.isLeaf) {
                    float baseScale = debris.startScale;

                    if (jumpOffset > 0.0f) {
                        float spinSpeed = 4.0f + std::fmod(debris.randomSeed, 3.0f);
                        float flipEffect = std::cos(timeActive * spinSpeed);
                        spr.setScale(baseScale * flipEffect, baseScale);

                        float tilt = std::sin(timeActive * 3.5f + debris.randomSeed) * 45.0f;
                        spr.setRotation(tilt);
                    } else {
                        spr.setScale(baseScale, baseScale);
                        auto& dVel = registry.get<ecs::Velocity>(e);
                        spr.setRotation(dVel.facingAngle);
                    }
                } else {
                    float scale = debris.startScale;
                    auto& dVel = registry.get<ecs::Velocity>(e);

                    spr.setRotation(dVel.facingAngle);
                    spr.setScale(scale, scale);
                }

                sf::Uint8 alpha = static_cast<sf::Uint8>(255.0f * (lifeRatio < 0.2f ? lifeRatio * 5.0f : 1.0f));
                spr.setColor(sf::Color(255, 255, 255, alpha));
                spr.setPosition(sp.x, sp.y - zOffset - jumpOffset);
                ctx.window.draw(spr);

            } else if (isBuilding) {
                spr.setScale(1.8f, 1.8f);
                spr.setPosition(sp.x, sp.y - zOffset);

                if (registry.all_of<ecs::ConstructionData>(e)) {
                    const auto& cData = registry.get<ecs::ConstructionData>(e);
                    if (!cData.isBuilt) {
                        float pct = std::clamp(cData.buildProgress / cData.maxTime, 0.0f, 1.0f);

                        spr.setColor(sf::Color(255, 255, 255, 100));
                        ctx.window.draw(spr);

                        if (pct > 0.0f) {
                            sf::Sprite solidSpr = spr;
                            solidSpr.setColor(sf::Color(255, 255, 255, 255));

                            auto bnds = solidSpr.getLocalBounds();
                            int fullHeight = static_cast<int>(bnds.height);
                            int clipHeight = static_cast<int>(fullHeight * pct);
                            int topOffset = fullHeight - clipHeight;

                            sf::IntRect currentRect = solidSpr.getTextureRect();
                            currentRect.top += topOffset;
                            currentRect.height = clipHeight;

                            solidSpr.setTextureRect(currentRect);
                            solidSpr.setOrigin(spr.getOrigin().x, spr.getOrigin().y - topOffset);
                            ctx.window.draw(solidSpr);
                        }

                        float pulse = (std::sin(ctx.timeSeconds * 6.0f) + 1.0f) * 0.5f;
                        sf::Uint8 glowAlpha = static_cast<sf::Uint8>(pulse * 140.0f);
                        sf::Sprite glowSpr = spr;
                        glowSpr.setColor(sf::Color(255, 255, 255, glowAlpha));
                        sf::RenderStates glowStates;
                        glowStates.blendMode = sf::BlendAdd;
                        ctx.window.draw(glowSpr, glowStates);
                    } else {
                        spr.setColor(sf::Color(255, 255, 255, 255));
                        ctx.window.draw(spr);
                    }
                } else {
                    ctx.window.draw(spr);
                }
            } else {
                if (registry.all_of<ecs::SelectedTag>(e)) {
                    sf::Sprite selMarker(ctx.tiles.getSelectionMarkerTexture());
                    selMarker.setOrigin(64.0f, 64.0f);
                    float markerZ = std::max(0.0f, zOffset);
                    selMarker.setPosition(sp.x, sp.y - markerZ);
                    selMarker.setRotation(0.0f);
                    selMarker.setScale(0.5f * ctx.pulseScale, 0.5f * ctx.pulseScale);
                    ctx.window.draw(selMarker);
                }

                float bobbing = 0.0f;
                if (isOnWater) {
                    if (const auto* vel = registry.try_get<ecs::Velocity>(e)) {
                        if (std::abs(vel->dx) > 0.01f || std::abs(vel->dy) > 0.01f) {
                            bobbing = std::abs(std::sin(ctx.timeSeconds * 12.0f)) * 6.0f;
                        }
                    }
                }

                float totalZ = zOffset + jumpOffset + bobbing;
                spr.setPosition(sp.x, sp.y - totalZ);
                spr.setRotation(0.0f);

                sf::IntRect currentRect = spr.getTextureRect();
                int texX = currentRect.left;
                int texY = currentRect.top;
                int texW = 192;
                int texH = 192;

                float cutLineY = sp.y + 16.0f;

                if (isOnWater) {
                    float unitVisualY = sp.y - totalZ;
                    float bottomY = unitVisualY + 96.0f;
                    if (bottomY > cutLineY) {
                        texH = std::max(0, static_cast<int>(192.0f - (bottomY - cutLineY)));
                    }
                }

                spr.setTextureRect(sf::IntRect(texX, texY, texW, texH));
                ctx.window.draw(spr);
            }
        }

        if (showDebug && !isDebris) {
            flushBatches();
            EntityDebugRenderer::drawShapes(ctx, registry, e, sp.x, sp.y, zOffset);
        }
    }

    flushBatches();
}

}