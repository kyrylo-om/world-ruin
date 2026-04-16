#include "Rendering/TileHandler.hpp"
#include <iostream>
#include <string>

namespace wr::rendering {

TileHandler::TileHandler() {
    m_grassOverWater = { get(1,1), get(1,2), get(1,3), get(2,1), get(2,2), get(2,3), get(3,1), get(3,2), get(3,3), get(4,1), get(4,2), get(4,3), get(1,4), get(2,4), get(3,4), get(4,4) };
    m_grassOverStone = { get(1,6), get(1,7), get(1,8), get(2,6), get(2,7), get(2,8), get(3,6), get(3,7), get(3,8), get(4,6), get(4,7), get(4,8), get(1,9), get(2,9), get(3,9), get(4,9) };
    m_stoneCliffs = { get(5,6), get(5,7), get(5,8), get(5,9) };
    m_waterCliffs = { get(6,6), get(6,7), get(6,8), get(6,9) };
    m_ramps = { get(5,1), get(6,1), get(5,4), get(6,4) };
}

bool TileHandler::init() noexcept {
    if (!m_texWater.loadFromFile("pack/Tiny Swords (Free Pack)/Terrain/Tileset/Water_Ocean.png")) return false;
    if (!m_texRiver.loadFromFile("pack/Tiny Swords (Free Pack)/Terrain/Tileset/Water_River.png")) return false;
    if (!m_texFoam.loadFromFile("pack/Tiny Swords (Free Pack)/Terrain/Tileset/Water Foam.png")) return false;
    if (!m_texShadow.loadFromFile("pack/Tiny Swords (Free Pack)/Terrain/Tileset/Shadow.png")) return false;

    if (!m_texCursor.loadFromFile("pack/Tiny Swords (Free Pack)/UI Elements/UI Elements/Cursors/Cursor_01.png")) return false;
    if (!m_texCursorHover.loadFromFile("pack/Tiny Swords (Free Pack)/UI Elements/UI Elements/Cursors/Cursor_02.png")) return false;
    if (!m_texSelectionMarker.loadFromFile("pack/Tiny Swords (Free Pack)/UI Elements/UI Elements/Cursors/Cursor_04.png")) return false;

    if (!m_texAvatarWarrior.loadFromFile("pack/Tiny Swords (Free Pack)/UI Elements/UI Elements/Human Avatars/Avatars_01.png")) return false;
    if (!m_texAvatarWorker.loadFromFile("pack/Tiny Swords (Free Pack)/UI Elements/UI Elements/Human Avatars/Avatars_05.png")) return false;
    if (!m_texAvatarCourier.loadFromFile("pack/Tiny Swords (Free Pack)/UI Elements/UI Elements/Human Avatars/Avatars_05.png")) return false;

    if (!m_texBigBarBase.loadFromFile("pack/Tiny Swords (Free Pack)/UI Elements/UI Elements/Bars/BigBar_Base.png")) return false;
    if (!m_texBigBarFill.loadFromFile("pack/Tiny Swords (Free Pack)/UI Elements/UI Elements/Bars/BigBar_Fill.png")) return false;
    if (!m_texSmallBarBase.loadFromFile("pack/Tiny Swords (Free Pack)/UI Elements/UI Elements/Bars/SmallBar_Base.png")) return false;
    if (!m_texSmallBarFill.loadFromFile("pack/Tiny Swords (Free Pack)/UI Elements/UI Elements/Bars/SmallBar_Fill.png")) return false;

    if (!m_texBannerSlots.loadFromFile("pack/Tiny Swords (Free Pack)/UI Elements/UI Elements/Banners/Banner_Slots.png")) return false;
    if (!m_texBadgeRed.loadFromFile("pack/Tiny Swords (Free Pack)/UI Elements/UI Elements/Buttons/TinyRoundRedButton.png")) return false;
    if (!m_texBadgeBlue.loadFromFile("pack/Tiny Swords (Free Pack)/UI Elements/UI Elements/Buttons/TinyRoundBlueButton.png")) return false;

    m_texBigBarFill.setRepeated(true);
    m_texSmallBarFill.setRepeated(true);

    if (!m_texAtlas.loadFromFile("pack/Tiny Swords (Free Pack)/Terrain/Tileset/Tilemap_combined.png")) return false;
    if (!m_resourcesAtlas.loadFromFile("pack/Tiny Swords (Free Pack)/Terrain/ResourcesAtlas.png")) return false;

    for (int i = 0; i < 4; ++i) {
        std::string path = "pack/Tiny Swords (Free Pack)/Terrain/Resources/Wood/Trees/Hit/Leaf Tree " + std::to_string(i + 1) + ".png";
        if (!m_texLeafTrees[i].loadFromFile(path)) return false;
    }
    if (!m_texCone.loadFromFile("pack/Tiny Swords (Free Pack)/Terrain/Resources/Wood/Trees/Hit/Cone.png")) return false;
    if (!m_bushLeafTexture.loadFromFile("pack/Tiny Swords (Free Pack)/Terrain/Decorations/Bushes/Hit/Leaf.png")) return false;
    if (!m_texWoodResource.loadFromFile("pack/Tiny Swords (Free Pack)/Terrain/Resources/Wood/Wood Resource/Wood Resource.png")) return false;

    if (!m_texHouse1.loadFromFile("pack/Tiny Swords (Free Pack)/Buildings/Blue Buildings/House1.png")) return false;

    if (!m_texWarriorIdle.loadFromFile("pack/Tiny Swords (Free Pack)/Units/Blue Units/Warrior/Warrior_Idle.png")) return false;
    if (!m_texWarriorRun.loadFromFile("pack/Tiny Swords (Free Pack)/Units/Blue Units/Warrior/Warrior_Run.png")) return false;
    if (!m_texWarriorAttack1.loadFromFile("pack/Tiny Swords (Free Pack)/Units/Blue Units/Warrior/Warrior_Attack1.png")) return false;
    if (!m_texWarriorAttack2.loadFromFile("pack/Tiny Swords (Free Pack)/Units/Blue Units/Warrior/Warrior_Attack2.png")) return false;

    if (!m_texPawnIdleAxe.loadFromFile("pack/Tiny Swords (Free Pack)/Units/Blue Units/Pawn/Pawn_Idle Axe.png")) return false;
    if (!m_texPawnRunAxe.loadFromFile("pack/Tiny Swords (Free Pack)/Units/Blue Units/Pawn/Pawn_Run Axe.png")) return false;
    if (!m_texPawnAttackAxe.loadFromFile("pack/Tiny Swords (Free Pack)/Units/Blue Units/Pawn/Pawn_Interact Axe.png")) return false;

    if (!m_texPawnIdlePickaxe.loadFromFile("pack/Tiny Swords (Free Pack)/Units/Blue Units/Pawn/Pawn_Idle Pickaxe.png")) return false;
    if (!m_texPawnRunPickaxe.loadFromFile("pack/Tiny Swords (Free Pack)/Units/Blue Units/Pawn/Pawn_Run Pickaxe.png")) return false;
    if (!m_texPawnAttackPickaxe.loadFromFile("pack/Tiny Swords (Free Pack)/Units/Blue Units/Pawn/Pawn_Interact Pickaxe.png")) return false;

    if (!m_texPawnIdleHammer.loadFromFile("pack/Tiny Swords (Free Pack)/Units/Blue Units/Pawn/Pawn_Idle Hammer.png")) return false;
    if (!m_texPawnRunHammer.loadFromFile("pack/Tiny Swords (Free Pack)/Units/Blue Units/Pawn/Pawn_Run Hammer.png")) return false;
    if (!m_texPawnAttackHammer.loadFromFile("pack/Tiny Swords (Free Pack)/Units/Blue Units/Pawn/Pawn_Interact Hammer.png")) return false;

    if (!m_texPawnIdle.loadFromFile("pack/Tiny Swords (Free Pack)/Units/Blue Units/Pawn/Pawn_Idle.png")) return false;
    if (!m_texPawnRun.loadFromFile("pack/Tiny Swords (Free Pack)/Units/Blue Units/Pawn/Pawn_Run.png")) return false;
    if (!m_texPawnIdleWood.loadFromFile("pack/Tiny Swords (Free Pack)/Units/Blue Units/Pawn/Pawn_Idle Wood.png")) return false;
    if (!m_texPawnRunWood.loadFromFile("pack/Tiny Swords (Free Pack)/Units/Blue Units/Pawn/Pawn_Run Wood.png")) return false;

    if (!m_texPawnIdleRock.loadFromFile("pack/Tiny Swords (Free Pack)/Units/Blue Units/Pawn/Pawn_Idle Rock.png")) return false;
    if (!m_texPawnRunRock.loadFromFile("pack/Tiny Swords (Free Pack)/Units/Blue Units/Pawn/Pawn_Run Rock.png")) return false;

    if (!m_texWaterSplash.loadFromFile("pack/Tiny Swords (Free Pack)/Particle FX/Water Splash.png")) return false;

    m_texWater.setRepeated(true);
    m_texRiver.setRepeated(true);
    m_texFoam.setRepeated(true);

    return true;
}

sf::Vector2i TileHandler::getBiomeOffset(world::BiomeType type) const noexcept { switch(type) { case world::BiomeType::Plains: return {0,0}; case world::BiomeType::Plains2: return {576,0}; case world::BiomeType::Bush: return {0,384}; case world::BiomeType::SpineForest: return {576,384}; default: return {0,0}; } }
sf::IntRect TileHandler::Platform::getPiece(int tx, int ty) const noexcept { if(tx==0&&ty==0)return topLeft; if(tx==1&&ty==0)return topMid; if(tx==2&&ty==0)return topRight; if(tx==0&&ty==1)return midLeft; if(tx==1&&ty==1)return midMid; if(tx==2&&ty==1)return midRight; if(tx==0&&ty==2)return botLeft; if(tx==1&&ty==2)return botMid; return botRight; }
sf::IntRect TileHandler::Platform::getNarrow(int x, int y, bool isVertical) const noexcept { if(isVertical){if(y==0)return vertTop;if(y==2)return vertBot;return vertMid;}else{if(x==0)return horzLeft;if(x==2)return horzRight;return horzMid;} }
const sf::IntRect& TileHandler::Platform::getByMask(uint8_t mask) const noexcept { switch(mask){case None:return isolated;case North:return vertBot;case South:return vertTop;case East:return horzLeft;case West:return horzRight;case(North|South):return vertMid;case(East|West):return horzMid;case(South|East):return topLeft;case(South|West):return topRight;case(North|East):return botLeft;case(North|West):return botRight;case(South|East|West):return topMid;case(North|East|West):return botMid;case(North|South|East):return midLeft;case(North|South|West):return midRight;case(North|South|East|West):return midMid;default:return midMid;} }
sf::IntRect TileHandler::CliffSet::getPiece(bool hasLeft, bool hasRight) const noexcept { if(!hasLeft&&hasRight)return pLeft;if(hasLeft&&!hasRight)return pRight;if(!hasLeft&&!hasRight)return pIsolated;return pMid; }
sf::IntRect TileHandler::RampSet::getPiece(bool isLeft, bool isTop) const noexcept { if(isLeft)return isTop?leftTop:leftBot;else return isTop?rightTop:rightBot; }

}