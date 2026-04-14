#pragma once
#include "ECS/Components.hpp"

namespace wr::simulation {
    class CombatAnimation {
    public:
        static void processTriggers(ecs::AnimationState& anim, const ecs::UnitData& uData, bool isAttackingCommand);
    };
}