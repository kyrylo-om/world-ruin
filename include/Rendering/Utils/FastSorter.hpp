#pragma once

#include <vector>
#include <entt/entt.hpp>

namespace wr::rendering {

    class FastSorter {
    public:

        static void sortByScreenY(std::vector<entt::entity>& entities, entt::registry& registry);
    };

}