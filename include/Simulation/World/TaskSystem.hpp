#pragma once
#include <entt/entt.hpp>

namespace wr::simulation {
    class TaskSystem {
    public:
        static void update(entt::registry& registry);
    };
}