#pragma once

#include "ECS/Components.hpp"
#include <entt/entt.hpp>

namespace wr::input {
    class TaskFinalizer {
    public:
        static void cancelTask(entt::registry& registry);
        static void finalizeTask(entt::registry& registry, ecs::PendingTaskArea& pArea, int& globalTaskId);
    };
}