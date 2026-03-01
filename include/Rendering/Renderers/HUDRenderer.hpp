#pragma once

#include "../RenderContext.hpp"
#include <entt/entt.hpp>

namespace wr::rendering {

    class HUDRenderer {
    public:

        static void render(const RenderContext& ctx, entt::registry& registry);
    };

}