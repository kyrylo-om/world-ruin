#pragma once

#include "Core/Types.hpp"
#include "Rendering/Perspective.hpp"
#include "World/ChunkManager.hpp"
#include <entt/entt.hpp>

namespace wr::simulation {

    class UnitControlSystem {
    public:
        explicit UnitControlSystem(world::ChunkManager& chunkManager);

        void update(entt::registry& registry, float dt, rendering::ViewDirection viewDir, const math::Vec2f& mouseWorldPos, bool isRightClicking) noexcept;

    private:
        world::ChunkManager& m_chunkManager;
        bool m_prevSpacePressed{false};
    };

}