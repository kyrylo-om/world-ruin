#pragma once

#include "Core/Types.hpp"
#include "World/Biomes.hpp"
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <array>

namespace wr::rendering {

    enum NeighborMask : uint8_t { None=0, North=1<<0, East=1<<1, South=1<<2, West=1<<3 };

    class TileHandler {
    public:
        static constexpr int TILE_SIZE = 64;
        static constexpr int FOAM_FRAME_SIZE = 192;
        static constexpr int FOAM_FRAME_COUNT = 16;

        struct Platform {
            sf::IntRect topLeft, topMid, topRight, midLeft, midMid, midRight, botLeft, botMid, botRight, horzLeft, horzMid, horzRight, vertTop, vertMid, vertBot, isolated;
            [[nodiscard]] sf::IntRect getPiece(int tx, int ty) const noexcept;
            [[nodiscard]] sf::IntRect getNarrow(int x, int y, bool isVertical) const noexcept;
            [[nodiscard]] const sf::IntRect& getByMask(uint8_t mask) const noexcept;
        };

        struct CliffSet {
            sf::IntRect pLeft, pMid, pRight, pIsolated;
            [[nodiscard]] sf::IntRect getPiece(bool hasLeft, bool hasRight) const noexcept;
        };

        struct RampSet {
            sf::IntRect leftTop, leftBot, rightTop, rightBot;
            [[nodiscard]] sf::IntRect getPiece(bool isLeft, bool isTop) const noexcept;
        };

        TileHandler();
        [[nodiscard]] bool init() noexcept;

        [[nodiscard]] Platform getGrassOverWater() const noexcept { return m_grassOverWater; }
        [[nodiscard]] Platform getGrassOverStone() const noexcept { return m_grassOverStone; }
        [[nodiscard]] CliffSet getStoneCliffs() const noexcept { return m_stoneCliffs; }
        [[nodiscard]] CliffSet getWaterCliffs() const noexcept { return m_waterCliffs; }
        [[nodiscard]] RampSet getRamps() const noexcept { return m_ramps; }

        [[nodiscard]] const sf::Texture& getAtlasTexture() const noexcept { return m_texAtlas; }
        [[nodiscard]] sf::Vector2i getBiomeOffset(world::BiomeType type) const noexcept;

        [[nodiscard]] const sf::Texture& getWaterTexture() const noexcept { return m_texWater; }
        [[nodiscard]] const sf::Texture& getRiverTexture() const noexcept { return m_texRiver; }
        [[nodiscard]] const sf::Texture& getShadowTexture() const noexcept { return m_texShadow; }
        [[nodiscard]] const sf::Texture& getFoamTexture() const noexcept { return m_texFoam; }

        [[nodiscard]] const sf::Texture& getResourcesAtlas() const noexcept { return m_resourcesAtlas; }

        [[nodiscard]] const sf::Texture& getBushLeafTexture() const noexcept { return m_bushLeafTexture; }
        [[nodiscard]] const sf::Texture& getLeafTexture(uint8_t index) const noexcept { return m_texLeafTrees[index % 4]; }
        [[nodiscard]] const sf::Texture& getConeTexture() const noexcept { return m_texCone; }

        [[nodiscard]] const sf::Texture& getWoodResourceTexture() const noexcept { return m_texWoodResource; }

        [[nodiscard]] const sf::Texture& getCursorTexture() const noexcept { return m_texCursor; }
        [[nodiscard]] const sf::Texture& getCursorHoverTexture() const noexcept { return m_texCursorHover; }
        [[nodiscard]] const sf::Texture& getSelectionMarkerTexture() const noexcept { return m_texSelectionMarker; }

        [[nodiscard]] const sf::Texture& getAvatarWarrior() const noexcept { return m_texAvatarWarrior; }
        [[nodiscard]] const sf::Texture& getAvatarWorker() const noexcept { return m_texAvatarWorker; }
        [[nodiscard]] const sf::Texture& getAvatarCourier() const noexcept { return m_texAvatarCourier; }

        [[nodiscard]] const sf::Texture& getBigBarBaseTexture() const noexcept { return m_texBigBarBase; }
        [[nodiscard]] const sf::Texture& getBigBarFillTexture() const noexcept { return m_texBigBarFill; }
        [[nodiscard]] const sf::Texture& getSmallBarBaseTexture() const noexcept { return m_texSmallBarBase; }
        [[nodiscard]] const sf::Texture& getSmallBarFillTexture() const noexcept { return m_texSmallBarFill; }

        [[nodiscard]] const sf::Texture& getWarriorIdleTexture() const noexcept { return m_texWarriorIdle; }
        [[nodiscard]] const sf::Texture& getWarriorRunTexture() const noexcept { return m_texWarriorRun; }
        [[nodiscard]] const sf::Texture& getWarriorAttack1Texture() const noexcept { return m_texWarriorAttack1; }
        [[nodiscard]] const sf::Texture& getWarriorAttack2Texture() const noexcept { return m_texWarriorAttack2; }

        [[nodiscard]] const sf::Texture& getPawnIdleAxeTexture() const noexcept { return m_texPawnIdleAxe; }
        [[nodiscard]] const sf::Texture& getPawnRunAxeTexture() const noexcept { return m_texPawnRunAxe; }
        [[nodiscard]] const sf::Texture& getPawnAttackAxeTexture() const noexcept { return m_texPawnAttackAxe; }

        [[nodiscard]] const sf::Texture& getPawnIdlePickaxeTexture() const noexcept { return m_texPawnIdlePickaxe; }
        [[nodiscard]] const sf::Texture& getPawnRunPickaxeTexture() const noexcept { return m_texPawnRunPickaxe; }
        [[nodiscard]] const sf::Texture& getPawnAttackPickaxeTexture() const noexcept { return m_texPawnAttackPickaxe; }

        [[nodiscard]] const sf::Texture& getPawnIdleTexture() const noexcept { return m_texPawnIdle; }
        [[nodiscard]] const sf::Texture& getPawnRunTexture() const noexcept { return m_texPawnRun; }
        [[nodiscard]] const sf::Texture& getPawnIdleWoodTexture() const noexcept { return m_texPawnIdleWood; }
        [[nodiscard]] const sf::Texture& getPawnRunWoodTexture() const noexcept { return m_texPawnRunWood; }
        [[nodiscard]] const sf::Texture& getPawnIdleRockTexture() const noexcept { return m_texPawnIdleRock; }
        [[nodiscard]] const sf::Texture& getPawnRunRockTexture() const noexcept { return m_texPawnRunRock; }

        [[nodiscard]] const sf::Texture& getWaterSplashTexture() const noexcept { return m_texWaterSplash; }

    private:
        sf::Texture m_texWater, m_texRiver, m_texFoam, m_texShadow, m_texAtlas;
        sf::Texture m_resourcesAtlas;

        std::array<sf::Texture, 4> m_texLeafTrees;
        sf::Texture m_texCone;
        sf::Texture m_bushLeafTexture;
        sf::Texture m_texWoodResource;

        sf::Texture m_texCursor, m_texCursorHover, m_texSelectionMarker;

        sf::Texture m_texAvatarWarrior, m_texAvatarWorker, m_texAvatarCourier;
        sf::Texture m_texBigBarBase, m_texBigBarFill, m_texSmallBarBase, m_texSmallBarFill;

        sf::Texture m_texWarriorIdle, m_texWarriorRun;
        sf::Texture m_texWarriorAttack1, m_texWarriorAttack2;

        sf::Texture m_texPawnIdleAxe, m_texPawnRunAxe, m_texPawnAttackAxe;
        sf::Texture m_texPawnIdlePickaxe, m_texPawnRunPickaxe, m_texPawnAttackPickaxe;

        sf::Texture m_texPawnIdle, m_texPawnRun;
        sf::Texture m_texPawnIdleWood, m_texPawnRunWood;
        sf::Texture m_texPawnIdleRock, m_texPawnRunRock;

        sf::Texture m_texWaterSplash;

        Platform m_grassOverWater, m_grassOverStone;
        CliffSet m_stoneCliffs, m_waterCliffs;
        RampSet m_ramps;

        [[nodiscard]] static inline sf::IntRect get(int row, int col) noexcept {
            return sf::IntRect((col - 1) * TILE_SIZE, (row - 1) * TILE_SIZE, TILE_SIZE, TILE_SIZE);
        }
    };

}