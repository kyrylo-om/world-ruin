#include "../../../../include/Simulation/Unit/Combat/CombatAnimation.hpp"

namespace wr::simulation {

    void CombatAnimation::processTriggers(ecs::AnimationState& anim, const ecs::UnitData& uData, bool isAttackingCommand) {
        if (isAttackingCommand) {
            if (anim.currentAnim == 0 || anim.currentAnim == 1) {
                anim.currentAnim = 2;
                anim.currentFrame = 0;
                anim.elapsedTime = 0.0f;
                anim.isActionLocked = true;

                anim.numFrames = (uData.type == ecs::UnitType::Warrior) ? 4 : 6;
                if (uData.type == ecs::UnitType::Courier) anim.numFrames = 4;
                if (uData.type == ecs::UnitType::Builder) anim.numFrames = 3;

                anim.damageDealt = false;
            } else if ((anim.currentAnim == 2 || anim.currentAnim == 3) && anim.currentFrame >= 2) {
                if (uData.type == ecs::UnitType::Warrior) anim.comboRequested = true;
            }
        }
    }

}