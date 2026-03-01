#pragma once

#include "Core/Math.hpp"
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <entt/entt.hpp>
#include <vector>

namespace wr::ecs {

    enum class UnitType { Warrior, Lumberjack, Miner, Courier };
    enum class HeldItem : uint8_t { None, Wood, Rock };

    struct UnitData {
        UnitType type;
        int damage;
        float attackRadius;
        float arcDegrees;
        HeldItem heldItem{HeldItem::None};
        uint8_t heldItemSubtype{0};
        entt::entity currentTask{entt::null};
    };

    struct LogicalPos { int64_t x, y; };

    struct WorldPos {
        float wx, wy;
        float wz{0.0f}, targetZ{0.0f}, zJump{0.0f}, zJumpVel{0.0f};
        bool isOnWater{false}, wasOnWater{false};
    };

    struct ScreenPos {
        float x, y;
        float targetAlpha{255.0f}, currentAlpha{255.0f};
        uint8_t flags{0};
    };

    struct Velocity {
        float dx{0.0f}, dy{0.0f};
        float baseSpeed{4.5f};
        float facingAngle{0.0f};
    };

    struct SpriteComponent { sf::Sprite sprite; };

    struct AnimationState {
        float elapsedTime{0.0f}, frameDuration{0.1f};
        int frameWidth{192}, frameHeight{192}, numFrames{8}, currentFrame{0}, currentAnim{0};
        bool isActionLocked{false}, comboRequested{false}, damageDealt{false};
    };

    struct Health { int current, max; };
    struct Hitbox { float radius; };
    struct ClickArea { float radius; };

    struct CachedQuad {
        sf::Vertex localVertices[4];
        const sf::Texture* texture{nullptr};
    };

    struct CachedCanopyQuad {
        sf::Vertex localVertices[4];
        const sf::Texture* texture{nullptr};
    };

    struct ResourceData {
        uint8_t type;
        bool isDestroyed{false};
        float shakeTimer{0.0f};
    };

    struct DebrisData {
        float lifeTimer, maxLife, startScale, gravity, randomSeed;
        bool isLeaf;
    };

    struct Path {
        std::vector<math::Vec2f> waypoints;
        size_t currentIndex{0};
    };

    struct ActionTarget { entt::entity target; };

    struct TaskArea {
        math::Vec2f startWorld;
        math::Vec2f endWorld;
        sf::Color color;
        int id;
        bool hasDropoff{false};
        math::Vec2f dropoffStart{0.0f, 0.0f};
        math::Vec2f dropoffEnd{0.0f, 0.0f};
        bool collectFutureDrops{false};
    };

    struct PendingTaskArea {
        math::Vec2f startWorld;
        math::Vec2f endWorld;

        bool canHarvestTrees{false};
        bool canHarvestRocks{false};
        bool canHarvestSmallRocks{false};
        bool canHarvestBushes{false};
        bool canHarvestLogs{false};

        bool includeTrees{true};
        bool includeRocks{true};
        bool includeSmallRocks{true};
        bool includeBushes{true};
        bool includeLogs{true};

        bool isSelectingDropoff{false};
        bool hasDropoff{false};
        math::Vec2f dropoffStart{0.0f, 0.0f};
        math::Vec2f dropoffEnd{0.0f, 0.0f};
        bool collectFutureDrops{false};
    };

}