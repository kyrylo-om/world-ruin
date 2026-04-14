#pragma once

#include "../RenderContext.hpp"
#include <entt/entt.hpp>
#include <vector>

namespace wr::rendering {

    class EntityRenderer {
    public:

        static void renderEntities(const RenderContext& ctx, entt::registry& registry, std::vector<entt::entity>& entities, bool skipSort = false, bool showDebug = false);
    };

}