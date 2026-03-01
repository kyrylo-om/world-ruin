#include <SFML/Graphics.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <entt/entt.hpp>
#include <iostream>
#include <iomanip>

#include "Core/ThreadPool.hpp"
#include "Core/Types.hpp"
#include "Core/Profiler.hpp"
#include "../include/World/Generation/WorldGenerator.hpp"
#include "World/ChunkManager.hpp"
#include "Rendering/TileHandler.hpp"
#include "Rendering/Camera.hpp"
#include "Rendering/Perspective.hpp"
#include "Rendering/RenderSystem.hpp"
#include "../include/Simulation/World/ChunkSimulation.hpp"
#include "../include/Simulation/Unit/UnitControlSystem.hpp"
#include "../include/Config/UnitConfig.hpp"
#include "Input/CursorHandler.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"

int main() {
    try {
        std::cout << "[INIT] Initializing World Ruin Engine..." << std::endl;

        sf::RenderWindow window(sf::VideoMode(1280, 720), "World Ruin - Selection & RTS Controls");
        window.setFramerateLimit(144);

        wr::rendering::TileHandler tileHandler;
        if (!tileHandler.init()) return -1;

        entt::registry registry;
        registry.ctx().emplace<const wr::rendering::TileHandler*>(&tileHandler);

        wr::input::CursorHandler cursor;
        cursor.init(tileHandler.getCursorTexture());
        cursor.setSystemCursorVisible(window, false);

        wr::core::ThreadPool threadPool;
        wr::world::WorldGenerator worldGen(12345);
        wr::world::ChunkManager chunkManager(threadPool, registry, worldGen, tileHandler);
        wr::simulation::ChunkSimulation chunkSim(threadPool, registry, chunkManager);
        wr::simulation::UnitControlSystem unitCtrl(chunkManager);

        wr::rendering::Camera camera(1280.0f, 720.0f);
        wr::rendering::RenderSystem renderSystem(5000);
        wr::rendering::ViewDirection currentViewDir = wr::rendering::ViewDirection::North;

        for (int i = 0; i < 6; ++i) {
            auto entity = registry.create();
            registry.emplace<wr::ecs::LogicalPos>(entity, 10 + i * 2, 10 + (i % 2));
            registry.emplace<wr::ecs::WorldPos>(entity, 10.0f + i * 2.0f, 10.0f + (i % 2), 0.0f, 0.0f);
            registry.emplace<wr::ecs::ScreenPos>(entity);
            registry.emplace<wr::ecs::Velocity>(entity);
            registry.emplace<wr::ecs::UnitTag>(entity);
            auto& anim = registry.emplace<wr::ecs::AnimationState>(entity, 0.0f, 0.1f, 192, 192, 6, 0);

            auto& spr = registry.emplace<wr::ecs::SpriteComponent>(entity).sprite;
            spr.setOrigin(96.0f, 112.0f);
            spr.setTextureRect(sf::IntRect(0, 0, 192, 192));

            if (i == 0 || i == 1) {
                registry.emplace<wr::ecs::UnitData>(entity, wr::ecs::UnitType::Warrior,
                                                    wr::simulation::config::WARRIOR_DAMAGE,
                                                    wr::simulation::config::WARRIOR_RANGE,
                                                    wr::simulation::config::WARRIOR_ARC,
                                                    wr::ecs::HeldItem::None, uint8_t{0});
                registry.emplace<wr::ecs::Health>(entity, wr::simulation::config::WARRIOR_HP, wr::simulation::config::WARRIOR_HP);
                spr.setTexture(tileHandler.getWarriorIdleTexture());
            } else if (i == 2) {
                registry.emplace<wr::ecs::UnitData>(entity, wr::ecs::UnitType::Lumberjack,
                                                    wr::simulation::config::LUMBERJACK_DAMAGE,
                                                    wr::simulation::config::LUMBERJACK_RANGE,
                                                    wr::simulation::config::LUMBERJACK_ARC,
                                                    wr::ecs::HeldItem::None, uint8_t{0});
                registry.emplace<wr::ecs::Health>(entity, wr::simulation::config::LUMBERJACK_HP, wr::simulation::config::LUMBERJACK_HP);
                spr.setTexture(tileHandler.getPawnIdleAxeTexture());
            } else if (i == 3) {
                registry.emplace<wr::ecs::UnitData>(entity, wr::ecs::UnitType::Miner,
                                                    wr::simulation::config::MINER_DAMAGE,
                                                    wr::simulation::config::MINER_RANGE,
                                                    wr::simulation::config::MINER_ARC,
                                                    wr::ecs::HeldItem::None, uint8_t{0});
                registry.emplace<wr::ecs::Health>(entity, wr::simulation::config::MINER_HP, wr::simulation::config::MINER_HP);
                spr.setTexture(tileHandler.getPawnIdlePickaxeTexture());
            } else {
                registry.emplace<wr::ecs::UnitData>(entity, wr::ecs::UnitType::Courier,
                                                    wr::simulation::config::COURIER_DAMAGE,
                                                    wr::simulation::config::COURIER_RANGE,
                                                    wr::simulation::config::COURIER_ARC,
                                                    wr::ecs::HeldItem::None, uint8_t{0});
                registry.emplace<wr::ecs::Health>(entity, wr::simulation::config::COURIER_HP, wr::simulation::config::COURIER_HP);
                spr.setTexture(tileHandler.getPawnIdleTexture());
            }
        }

        sf::Clock clock;
        sf::Clock statsClock;
        int frames = 0;

        while (window.isOpen()) {
            float dt = clock.restart().asSeconds();
            sf::Event ev;

            while (window.pollEvent(ev)) {
                if (ev.type == sf::Event::Closed) window.close();
                camera.handleEvent(ev);

                if (ev.type == sf::Event::MouseButtonPressed && ev.mouseButton.button == sf::Mouse::Left) {
                    cursor.startDrag();
                }

                if (ev.type == sf::Event::MouseButtonReleased && ev.mouseButton.button == sf::Mouse::Left) {
                    bool wasValidDrag = cursor.endDrag(registry, camera, chunkManager);
                    if (!wasValidDrag) {
                        cursor.handleMouseClick(registry, window, camera, chunkManager);
                    }
                }

                if (ev.type == sf::Event::KeyPressed) {
                    if (ev.key.code == sf::Keyboard::C) {
                        camera.rotate(90.0f);
                        int dir = static_cast<int>(currentViewDir);
                        currentViewDir = static_cast<wr::rendering::ViewDirection>((dir + 1) % 4);
                    }
                    if (ev.key.code == sf::Keyboard::Z) {
                        camera.rotate(-90.0f);
                        int dir = static_cast<int>(currentViewDir);
                        currentViewDir = static_cast<wr::rendering::ViewDirection>((dir + 3) % 4);
                    }
                    if (ev.key.code == sf::Keyboard::L) renderSystem.toggleDebug();

                    if (ev.key.code == sf::Keyboard::Escape) {
                        registry.clear<wr::ecs::SelectedTag>();
                        camera.clearFollowTarget();
                    }
                }
            }

            bool isRightClick = sf::Mouse::isButtonPressed(sf::Mouse::Right);
            unitCtrl.update(registry, dt, currentViewDir, cursor.getExactHoveredWorldPos(), isRightClick);

            auto selectedView = registry.view<wr::ecs::SelectedTag, wr::ecs::WorldPos>();
            int selectedCount = std::distance(selectedView.begin(), selectedView.end());

            if (selectedCount == 1) {
                camera.setFollowTarget(&selectedView.get<wr::ecs::WorldPos>(*selectedView.begin()));
                camera.updateFollow(dt);
            } else {
                camera.clearFollowTarget();
                camera.updateFreeFly(
                    sf::Keyboard::isKeyPressed(sf::Keyboard::W),
                    sf::Keyboard::isKeyPressed(sf::Keyboard::S),
                    sf::Keyboard::isKeyPressed(sf::Keyboard::A),
                    sf::Keyboard::isKeyPressed(sf::Keyboard::D),
                    dt
                );
            }

            wr::math::Vec2i64 camAbs = camera.getAbsolutePosition();
            wr::math::Vec2f camPx = { static_cast<float>(camAbs.x) * 64.0f, static_cast<float>(camAbs.y) * 64.0f };

            chunkManager.update(camPx, currentViewDir);
            chunkSim.update(dt, currentViewDir);

            cursor.update(window, camera, chunkManager, registry);

            wr::rendering::Perspective::updateScreenPositions(registry, camera, currentViewDir);
            renderSystem.updateInterpolation(registry, dt);

            wr::rendering::ActiveDragData dragData;
            dragData.isDragging = cursor.isDragging();
            dragData.startWorld = cursor.getDragStartWorldPos();
            dragData.endWorld = cursor.getExactHoveredWorldPos();
            dragData.color = cursor.getDragColor();

            window.clear(sf::Color(10, 10, 15));
            renderSystem.render(registry, window, camera, tileHandler, chunkManager.getChunks(), currentViewDir, dragData);

            cursor.draw(window);
            window.display();

            frames++;
            if (statsClock.getElapsedTime().asSeconds() >= 1.0f) {
                std::cout << "\r[STATS] FPS: " << frames << " | Entities: " << registry.alive() << "    " << std::flush;
                frames = 0; statsClock.restart();
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "FATAL ERROR: " << e.what() << std::endl;
        return -1;
    }
    return 0;
}