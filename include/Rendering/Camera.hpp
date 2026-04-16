#pragma once

#include "../Core/Math.hpp"
#include "../ECS/Components.hpp"
#include <SFML/Graphics/View.hpp>
#include <SFML/Window/Event.hpp>
#include <cstdint>

namespace wr::rendering {

    struct ChunkBounds {
        int64_t minX, maxX, minY, maxY;
    };

    class Camera {
    public:
        Camera(float width, float height);

        void handleEvent(const sf::Event& ev) noexcept;

        void updateFreeFly(bool isW, bool isS, bool isA, bool isD, float dt) noexcept;
        void updateFollow(float dt) noexcept;

        void rotate(float degrees) noexcept { m_targetRotation += degrees; }

        [[nodiscard]] math::Vec2f getRelativeScreenPosition(const math::Vec2i64& worldPos) const noexcept;
        [[nodiscard]] ChunkBounds getVisibleChunkBounds() const noexcept;

        void setFollowTarget(const ecs::WorldPos* target) noexcept { m_targetFollowPos = target; }
        void clearFollowTarget() noexcept { m_targetFollowPos = nullptr; }
        [[nodiscard]] bool isFollowing() const noexcept { return m_targetFollowPos != nullptr; }

        [[nodiscard]] const sf::View& getView() const noexcept { return m_view; }
        [[nodiscard]] math::Vec2i64 getAbsolutePosition() const noexcept { return m_absPos; }

        [[nodiscard]] float getSubPixelX() const noexcept { return m_subPixelX; }
        [[nodiscard]] float getSubPixelY() const noexcept { return m_subPixelY; }

        [[nodiscard]] float getRotation() const noexcept { return m_currentRotation; }
        [[nodiscard]] float getZoom() const noexcept { return m_zoomLevel; }

    private:
        void updateCommon(float dt) noexcept;
        void normalizePosition() noexcept;

        sf::View m_view;

        math::Vec2i64 m_absPos{0, 0};
        float m_subPixelX = 0.0f;
        float m_subPixelY = 0.0f;

        const ecs::WorldPos* m_targetFollowPos{nullptr};

        float m_followSpeed{6.0f};

        float m_currentRotation = 0.0f;
        float m_targetRotation = 0.0f;
        float m_rotationSpeed = 8.0f;

        float m_zoomLevel = 1.0f;
        float m_targetZoom = 1.0f;
    };

}