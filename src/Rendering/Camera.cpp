#include "Rendering/Camera.hpp"
#include <algorithm>
#include <cmath>
#include <SFML/Window/Keyboard.hpp>

namespace wr::rendering {

static constexpr float MIN_ZOOM = 0.5f;
static constexpr float MAX_ZOOM = 5.0f;
static constexpr float ZOOM_SPEED = 10.0f;
static constexpr float MOVE_BASE_SPEED = 500.0f;

Camera::Camera(float width, float height) {
    m_view.setSize(width, height);
    m_view.setCenter(0.0f, 0.0f);
}

void Camera::handleEvent(const sf::Event& ev) noexcept {
    if (ev.type == sf::Event::MouseWheelScrolled) {
        float delta = ev.mouseWheelScroll.delta;
        m_targetZoom = std::clamp(m_targetZoom * (delta > 0 ? 0.85f : 1.15f), MIN_ZOOM, MAX_ZOOM);
    }
}

void Camera::updateCommon(float dt) noexcept {
    if (std::abs(m_targetRotation - m_currentRotation) > 0.01f) {
        m_currentRotation = math::lerp(m_currentRotation, m_targetRotation, m_rotationSpeed * dt);
    } else {
        m_currentRotation = m_targetRotation;
    }

    if (std::abs(m_targetZoom - m_zoomLevel) > 0.001f) {
        m_zoomLevel = math::lerp(m_zoomLevel, m_targetZoom, ZOOM_SPEED * dt);
        m_view.setSize(1280.0f * m_zoomLevel, 720.0f * m_zoomLevel);
    }
}

void Camera::normalizePosition() noexcept {
    while (m_subPixelX > 64.0f) { m_subPixelX -= 64.0f; m_absPos.x++; }
    while (m_subPixelX < 0.0f)  { m_subPixelX += 64.0f; m_absPos.x--; }
    while (m_subPixelY > 64.0f) { m_subPixelY -= 64.0f; m_absPos.y++; }
    while (m_subPixelY < 0.0f)  { m_subPixelY += 64.0f; m_absPos.y--; }
}

void Camera::updateFreeFly(bool isW, bool isS, bool isA, bool isD, float dt) noexcept {
    updateCommon(dt);

    float moveSpeed = MOVE_BASE_SPEED * m_zoomLevel * dt;
    float rad = m_currentRotation * 3.14159265f / 180.0f;
    sf::Vector2f dirForward = { -std::sin(rad), -std::cos(rad) };
    sf::Vector2f dirRight   = {  std::cos(rad), -std::sin(rad) };

    if (isW) { m_subPixelX += dirForward.x * moveSpeed; m_subPixelY += dirForward.y * moveSpeed; }
    if (isS) { m_subPixelX -= dirForward.x * moveSpeed; m_subPixelY -= dirForward.y * moveSpeed; }
    if (isA) { m_subPixelX -= dirRight.x * moveSpeed; m_subPixelY -= dirRight.y * moveSpeed; }
    if (isD) { m_subPixelX += dirRight.x * moveSpeed; m_subPixelY += dirRight.y * moveSpeed; }

    normalizePosition();
}

void Camera::updateFollow(float dt) noexcept {
    updateCommon(dt);

    if (m_targetFollowPos) {
        float camWorldX = static_cast<float>(m_absPos.x) + (m_subPixelX / 64.0f);
        float camWorldY = static_cast<float>(m_absPos.y) + (m_subPixelY / 64.0f);

        float targetWorldX = m_targetFollowPos->wx;
        float targetWorldY = m_targetFollowPos->wy;

        float rad = m_currentRotation * 3.14159265f / 180.0f;
        float dirForwardX = -std::sin(rad);
        float dirForwardY = -std::cos(rad);

        float verticalOffsetPx = m_targetFollowPos->wz + 32.0f;
        float verticalOffsetBlocks = verticalOffsetPx / 64.0f;

        targetWorldX += dirForwardX * verticalOffsetBlocks;
        targetWorldY += dirForwardY * verticalOffsetBlocks;

        camWorldX = math::lerp(camWorldX, targetWorldX, 12.0f * dt);
        camWorldY = math::lerp(camWorldY, targetWorldY, 12.0f * dt);

        m_absPos.x = static_cast<int64_t>(std::floor(camWorldX));
        m_absPos.y = static_cast<int64_t>(std::floor(camWorldY));
        m_subPixelX = (camWorldX - static_cast<float>(m_absPos.x)) * 64.0f;
        m_subPixelY = (camWorldY - static_cast<float>(m_absPos.y)) * 64.0f;
    }

    normalizePosition();
}

math::Vec2f Camera::getRelativeScreenPosition(const math::Vec2i64& worldPos) const noexcept {
    float dx = static_cast<float>(worldPos.x - m_absPos.x) * 64.0f;
    float dy = static_cast<float>(worldPos.y - m_absPos.y) * 64.0f;
    return { dx - m_subPixelX, dy - m_subPixelY };
}

ChunkBounds Camera::getVisibleChunkBounds() const noexcept {
    sf::Vector2f viewSize = m_view.getSize();
    double diagonal = std::sqrt((viewSize.x * viewSize.x) + (viewSize.y * viewSize.y));
    double radiusPx = diagonal / 2.0;

    double centerPxX = static_cast<double>(m_absPos.x) * 64.0 + m_subPixelX;
    double centerPxY = static_cast<double>(m_absPos.y) * 64.0 + m_subPixelY;

    double minPxX = centerPxX - radiusPx;
    double maxPxX = centerPxX + radiusPx;
    double minPxY = centerPxY - radiusPx;
    double maxPxY = centerPxY + radiusPx;

    auto toChunk = [](double px) -> int64_t { return static_cast<int64_t>(std::floor(px / 4096.0)); };

    return { toChunk(minPxX)-1, toChunk(maxPxX)+1, toChunk(minPxY)-1, toChunk(maxPxY)+1 };
}

}