#pragma once
#include <entt/entt.hpp>

namespace wr::simulation {
    class DebrisSystem {
    public:
        static void update(entt::registry& registry, float dt);
    };
}