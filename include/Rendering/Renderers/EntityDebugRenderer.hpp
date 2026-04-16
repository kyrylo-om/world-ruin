#pragma once

#include "../RenderContext.hpp"
#include <entt/entt.hpp>

namespace wr::rendering {

    class EntityDebugRenderer {
    public:
        static void drawShapes(const RenderContext& ctx, entt::registry& registry, entt::entity e, float spX, float spY, float zOffset);
    };

}