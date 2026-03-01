#pragma once

#include <SFML/Graphics/Color.hpp>
#include <entt/entt.hpp>

namespace wr::ecs {
    struct WaterTag {};
    struct GroundTag {};
    struct FoamTag {};
    struct UnitTag {};
    struct SelectedTag {};
    struct OneShotAnimTag {};

    struct ResourceTag {};
    struct RockTag {};
    struct SmallRockTag {};
    struct TreeTag {};
    struct BushTag {};
    struct LogTag {};

    struct WalkableTag {};
    struct SolidTag {};
    struct DebrisTag {};

    struct MarkedForHarvestTag {
        sf::Color color{255, 255, 255, 255};
        entt::entity taskEntity{entt::null};
    };

    struct PreviewHarvestTag {
        sf::Color color{255, 255, 255, 255};
    };

    struct ClaimedTag {};
    struct WorkerTag {};
}