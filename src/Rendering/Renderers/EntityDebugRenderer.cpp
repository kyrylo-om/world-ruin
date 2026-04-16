#include "Rendering/Renderers/EntityDebugRenderer.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/ConvexShape.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <cmath>

namespace wr::rendering {

void EntityDebugRenderer::drawShapes(const RenderContext& ctx, entt::registry& registry, entt::entity e, float spX, float spY, float zOffset) {
    bool isRock = registry.all_of<ecs::RockTag>(e);
    bool isSmallRock = registry.all_of<ecs::SmallRockTag>(e);
    bool isTree = registry.all_of<ecs::TreeTag>(e);
    bool isBush = registry.all_of<ecs::BushTag>(e);
    bool isLog  = registry.all_of<ecs::LogTag>(e);
    bool isBuilding = registry.all_of<ecs::BuildingTag>(e);

    if (registry.all_of<ecs::DebrisTag>(e)) return;

    float hitBoxRadius = 19.2f;
    if (const auto* hb = registry.try_get<ecs::Hitbox>(e)) hitBoxRadius = hb->radius;
    else if (const auto* ca = registry.try_get<ecs::ClickArea>(e)) hitBoxRadius = ca->radius;

    sf::RectangleShape collisionBox(sf::Vector2f(hitBoxRadius * 2.0f, hitBoxRadius * 2.0f));
    collisionBox.setOrigin(hitBoxRadius, hitBoxRadius);
    collisionBox.setPosition(spX, spY - zOffset);
    collisionBox.setRotation(ctx.currentAngle);

    if (isRock || isSmallRock || isTree || isBush || isLog || isBuilding) {
        collisionBox.setFillColor(sf::Color(255, 255, 0, 40));
        collisionBox.setOutlineColor(sf::Color::Yellow);
    } else {
        collisionBox.setFillColor(sf::Color(255, 0, 0, 40));
        collisionBox.setOutlineColor(sf::Color::Red);
    }
    collisionBox.setOutlineThickness(1.5f);
    ctx.window.draw(collisionBox);

    if (!isRock && !isSmallRock && !isTree && !isBush && !isLog && !isBuilding) {
        auto* uData = registry.try_get<ecs::UnitData>(e);
        auto* cStats = registry.try_get<ecs::CombatStats>(e);
        auto* vel = registry.try_get<ecs::Velocity>(e);

        if (uData && cStats && vel) {
            float attackRadius = cStats->attackRadius * 64.0f;
            float spreadAngle = cStats->arcDegrees;

            float renderAngle = vel->facingAngle + ctx.currentAngle;
            float startAngle = renderAngle - (spreadAngle / 2.0f);

            int numPoints = 20;
            sf::ConvexShape attackArc(numPoints + 2);
            attackArc.setPoint(0, sf::Vector2f(0.0f, 0.0f));

            for (int i = 0; i <= numPoints; ++i) {
                float a = startAngle + (spreadAngle * i / numPoints);
                float rad = a * 3.14159265f / 180.0f;
                attackArc.setPoint(i + 1, sf::Vector2f(std::cos(rad) * attackRadius, std::sin(rad) * attackRadius));
            }

            attackArc.setPosition(spX, spY - zOffset);
            attackArc.setFillColor(sf::Color(255, 165, 0, 60));
            attackArc.setOutlineColor(sf::Color(255, 165, 0, 200));
            attackArc.setOutlineThickness(1.5f);
            ctx.window.draw(attackArc);
        }

        if (auto* path = registry.try_get<ecs::Path>(e)) {
            if (path->currentIndex < path->waypoints.size()) {
                sf::VertexArray pathLines(sf::LineStrip);

                pathLines.append(sf::Vertex(sf::Vector2f(spX, spY - zOffset), sf::Color::Magenta));

                for (size_t i = path->currentIndex; i < path->waypoints.size(); ++i) {
                    math::Vec2f w = path->waypoints[i];
                    float dx = (w.x - static_cast<float>(ctx.camera.getAbsolutePosition().x)) * 64.0f;
                    float dy = (w.y - static_cast<float>(ctx.camera.getAbsolutePosition().y)) * 64.0f;
                    math::Vec2f rel = { dx - ctx.camera.getSubPixelX(), dy - ctx.camera.getSubPixelY() };
                    float rad = ctx.currentAngle * 3.14159265f / 180.0f;
                    float rotX = rel.x * std::cos(rad) - rel.y * std::sin(rad);
                    float rotY = rel.x * std::sin(rad) + rel.y * std::cos(rad);

                    pathLines.append(sf::Vertex(sf::Vector2f(rotX, rotY - zOffset), sf::Color::Magenta));
                }
                ctx.window.draw(pathLines);

                math::Vec2f dest = path->waypoints.back();
                float dx = (dest.x - static_cast<float>(ctx.camera.getAbsolutePosition().x)) * 64.0f;
                float dy = (dest.y - static_cast<float>(ctx.camera.getAbsolutePosition().y)) * 64.0f;
                math::Vec2f rel = { dx - ctx.camera.getSubPixelX(), dy - ctx.camera.getSubPixelY() };
                float rad = ctx.currentAngle * 3.14159265f / 180.0f;
                float rotX = rel.x * std::cos(rad) - rel.y * std::sin(rad);
                float rotY = rel.x * std::sin(rad) + rel.y * std::cos(rad);

                sf::RectangleShape endDot(sf::Vector2f(8.0f, 8.0f));
                endDot.setOrigin(4.0f, 4.0f);
                endDot.setPosition(rotX, rotY - zOffset);
                endDot.setFillColor(sf::Color::Red);
                ctx.window.draw(endDot);
            }
        }

        if (registry.all_of<ecs::PathRequest>(e)) {
            sf::RectangleShape waitBox(sf::Vector2f(10.0f, 10.0f));
            waitBox.setOrigin(5.0f, 5.0f);
            waitBox.setPosition(spX, spY - zOffset - 40.0f);
            waitBox.setFillColor(sf::Color::Yellow);
            waitBox.setOutlineColor(sf::Color::Black);
            waitBox.setOutlineThickness(1.0f);
            ctx.window.draw(waitBox);
        }
    }

    sf::RectangleShape centerDot(sf::Vector2f(4.0f, 4.0f));
    centerDot.setOrigin(2.0f, 2.0f);
    centerDot.setPosition(spX, spY - zOffset);
    centerDot.setFillColor(sf::Color::Cyan);
    ctx.window.draw(centerDot);
}

}