#pragma once
#include "Core/Math.hpp"
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <entt/entt.hpp>
#include <vector>
#include <future>

namespace wr::ecs {

    enum class GameMode { Player, Simulation };

    enum class UnitType { Warrior, Lumberjack, Miner, Courier, Builder };
    enum class HeldItem : uint8_t { None, Wood, Rock };

    struct TaskModeState { bool active{false}; };

    struct BuilderModeState {
        bool active{false};
        int selectedBuilding{0};
        bool canPlace{false};
        math::Vec2f placementPos{0.0f, 0.0f};
    };

    struct UnitData {
        UnitType type;
        HeldItem heldItem{HeldItem::None};
        uint8_t heldItemSubtype{0};
    };

    struct CombatStats {
        int damage;
        float attackRadius;
        float arcDegrees;
    };

    struct WorkerState {
        entt::entity currentTask{entt::null};
        std::vector<entt::entity> taskQueue;
        float workTimer{0.0f};
        math::Vec2f workOffset{0.0f, 0.0f};
        int pathFailures{0};
        float directMoveTimer{0.0f};
        entt::entity lastFailedTarget{entt::null};
    };

    struct LogicalPos { int64_t x, y; };

    struct WorldPos {
        float wx, wy;
        float wz{0.0f}, targetZ{-9999.0f}, zJump{0.0f}, zJumpVel{0.0f};
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

    struct PathRequest { std::shared_future<std::vector<math::Vec2f>> futurePath; };

    struct Path {
        std::vector<math::Vec2f> waypoints;
        size_t currentIndex{0};
    };

    struct ActionTarget { entt::entity target; };

    struct RectArea {
        math::Vec2f startWorld;
        math::Vec2f endWorld;
        bool canHarvestTrees{false}, canHarvestRocks{false}, canHarvestSmallRocks{false}, canHarvestBushes{false}, canHarvestLogs{false};
        bool includeTrees{true}, includeRocks{true}, includeSmallRocks{true}, includeBushes{true}, includeLogs{true};
    };

    struct TaskArea {
        std::vector<RectArea> areas;
        std::vector<entt::entity> targetBuildings;
        sf::Color color;
        int id;
        bool hasDropoff{false};
        math::Vec2f dropoffStart{0.0f, 0.0f};
        math::Vec2f dropoffEnd{0.0f, 0.0f};
        bool collectFutureDrops{false};
        bool useCityStorage{false};
    };

    struct PendingTaskArea {
        std::vector<RectArea> areas;
        std::vector<entt::entity> targetBuildings;
        bool canHarvestTrees{false}, canHarvestRocks{false}, canHarvestSmallRocks{false}, canHarvestBushes{false}, canHarvestLogs{false};
        bool includeTrees{true}, includeRocks{true}, includeSmallRocks{true}, includeBushes{true}, includeLogs{true};
        bool hasDropoff{false};
        math::Vec2f dropoffStart{0.0f, 0.0f};
        math::Vec2f dropoffEnd{0.0f, 0.0f};
        bool collectFutureDrops{false};
        bool useCityStorage{false};
        float errorTimer{0.0f};
        int selectedAreaIndex{-1};
    };

    struct ConstructionData {
        int initialWood;
        int initialRock;
        int woodRequired;
        int rockRequired;
        float resourceZoneWidth;
        float resourceZoneHeight;
        bool isBuilt{false};
        float buildProgress{0.0f};
        float maxTime{30.0f};
    };

}