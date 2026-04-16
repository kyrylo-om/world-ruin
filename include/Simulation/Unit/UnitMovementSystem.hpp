#pragma once
#include "Core/Math.hpp"
#include "Rendering/Perspective.hpp"
#include "Simulation/Unit/UnitControlSystem.hpp"
#include "ECS/Components.hpp"
#include <entt/entt.hpp>

namespace wr::simulation {

    class UnitMovementSystem {
    public:
        static math::Vec2f getKeyboardWorldVector(rendering::ViewDirection viewDir);
        static void applyMovement(entt::entity entity, ecs::WorldPos& wp, ecs::Velocity& vel, ecs::AnimationState& anim, ecs::WorkerState& wState, ecs::Path* path, ecs::PathRequest* req, const math::Vec2f& moveIntent, DeferredQueues& dq);
    };

}