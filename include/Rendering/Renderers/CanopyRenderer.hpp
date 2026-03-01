#pragma once

#include "../RenderContext.hpp"
#include <entt/entt.hpp>
#include <vector>

namespace wr::rendering {

    class CanopyRenderer {
    public:
        static void render(const RenderContext& ctx, entt::registry& registry, const std::vector<entt::entity>& sortedTrees);
    };

}