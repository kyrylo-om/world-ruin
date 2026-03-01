#include "Rendering/Renderers/EntityDebugRenderer.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/ConvexShape.hpp>
#include <cmath>

namespace wr::rendering {

void EntityDebugRenderer::drawShapes(const RenderContext& ctx, entt::registry& registry, entt::entity e, float spX, float spY, float zOffset) {
    bool isRock = registry.all_of<ecs::RockTag>(e);
    bool isSmallRock = registry.all_of<ecs::SmallRockTag>(e);
    bool isTree = registry.all_of<ecs::TreeTag>(e);
    bool isBush = registry.all_of<ecs::BushTag>(e);
    bool isLog  = registry.all_of<ecs::LogTag>(e);
    if (registry.all_of<ecs::DebrisTag>(e)) return;

    float hitBoxRadius = 19.2f;
    if (const auto* hb = registry.try_get<ecs::Hitbox>(e)) hitBoxRadius = hb->radius;
    else if (const auto* ca = registry.try_get<ecs::ClickArea>(e)) hitBoxRadius = ca->radius;

    sf::RectangleShape collisionBox({hitBoxRadius * 2.0f, hitBoxRadius * 2.0f});
    collisionBox.setOrigin(hitBoxRadius, hitBoxRadius);
    collisionBox.setPosition(spX, spY - zOffset);
    collisionBox.setRotation(ctx.currentAngle);

    if (isRock || isSmallRock || isTree || isBush || isLog) {
        collisionBox.setFillColor(sf::Color(255, 255, 0, 40));
        collisionBox.setOutlineColor(sf::Color::Yellow);
    } else {
        collisionBox.setFillColor(sf::Color(255, 0, 0, 40));
        collisionBox.setOutlineColor(sf::Color::Red);
    }
    collisionBox.setOutlineThickness(1.5f);
    ctx.window.draw(collisionBox);

    if (!isRock && !isSmallRock && !isTree && !isBush && !isLog) {
        auto* uData = registry.try_get<ecs::UnitData>(e);
        auto* vel = registry.try_get<ecs::Velocity>(e);

        if (uData && vel) {
            float attackRadius = uData->attackRadius * 64.0f;
            float spreadAngle = uData->arcDegrees;

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
    }

    sf::RectangleShape centerDot({4.0f, 4.0f});
    centerDot.setOrigin(2.0f, 2.0f);
    centerDot.setPosition(spX, spY - zOffset);
    centerDot.setFillColor(sf::Color::Cyan);
    ctx.window.draw(centerDot);
}

}