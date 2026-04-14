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
#include "Input/CursorClickSystem.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include "Simulation/World/OverseerSystem.hpp"

#if defined(_WIN32)
extern "C" {
    __declspec(dllexport) uint32_t NvOptimusEnablement = 1;
    __declspec(dllexport) int AmdiDxXG11Hidden = 1;
}
#endif

static wr::ecs::GameMode showStartScreen(sf::RenderWindow& window) {
    sf::Font font;
    if (!font.loadFromFile("assets/fonts/PressStart2P-Regular.ttf")) {
        if (!font.loadFromFile("assets/fonts/Roboto-Bold.ttf")) {
            std::cerr << "[WARN] Could not load start screen font, defaulting to Player mode" << std::endl;
            return wr::ecs::GameMode::Player;
        }
    }

    sf::Text title("WORLD RUIN", font, 36);
    title.setFillColor(sf::Color(255, 215, 0));
    title.setOutlineColor(sf::Color::Black);
    title.setOutlineThickness(3.0f);

    sf::Text subtitle("Select Game Mode", font, 14);
    subtitle.setFillColor(sf::Color(200, 200, 200));

    sf::Text playerBtn("[ 1 ]  Player Mode", font, 16);
    sf::Text simBtn("[ 2 ]  Simulation Mode", font, 16);

    sf::Text playerDesc("Control units manually - harvest, build, and expand", font, 9);
    playerDesc.setFillColor(sf::Color(150, 150, 150));

    sf::Text simDesc("AI overseer controls everything autonomously", font, 9);
    simDesc.setFillColor(sf::Color(150, 150, 150));

    while (window.isOpen()) {
        sf::Event ev;
        while (window.pollEvent(ev)) {
            if (ev.type == sf::Event::Closed) {
                window.close();
                return wr::ecs::GameMode::Player;
            }
            if (ev.type == sf::Event::KeyPressed) {
                if (ev.key.code == sf::Keyboard::Num1) return wr::ecs::GameMode::Player;
                if (ev.key.code == sf::Keyboard::Num2) return wr::ecs::GameMode::Simulation;
            }
            if (ev.type == sf::Event::Resized) {
                sf::FloatRect visibleArea(0, 0, static_cast<float>(ev.size.width), static_cast<float>(ev.size.height));
                window.setView(sf::View(visibleArea));
            }
        }

        sf::Vector2f ws(static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y));
        float cx = ws.x / 2.0f;
        float cy = ws.y / 2.0f;

        auto centerText = [](sf::Text& t, float x, float y) {
            sf::FloatRect b = t.getLocalBounds();
            t.setOrigin(b.left + b.width / 2.0f, b.top + b.height / 2.0f);
            t.setPosition(x, y);
        };

        centerText(title, cx, cy - 120.0f);
        centerText(subtitle, cx, cy - 60.0f);

        playerBtn.setFillColor(sf::Color(100, 255, 100));
        playerBtn.setOutlineColor(sf::Color::Black);
        playerBtn.setOutlineThickness(2.0f);
        centerText(playerBtn, cx, cy + 10.0f);
        centerText(playerDesc, cx, cy + 40.0f);

        simBtn.setFillColor(sf::Color(100, 200, 255));
        simBtn.setOutlineColor(sf::Color::Black);
        simBtn.setOutlineThickness(2.0f);
        centerText(simBtn, cx, cy + 80.0f);
        centerText(simDesc, cx, cy + 110.0f);

        window.clear(sf::Color(15, 15, 25));
        window.draw(title);
        window.draw(subtitle);
        window.draw(playerBtn);
        window.draw(playerDesc);
        window.draw(simBtn);
        window.draw(simDesc);
        window.display();
    }

    return wr::ecs::GameMode::Player;
}

int main() {
    try {
        std::cout << "[INIT] Initializing World Ruin Engine..." << std::endl;

        sf::RenderWindow window(sf::VideoMode(1280, 720), "World Ruin");
        window.setFramerateLimit(144);

        wr::ecs::GameMode gameMode = showStartScreen(window);
        if (!window.isOpen()) return 0;

        bool isSimulation = (gameMode == wr::ecs::GameMode::Simulation);
        std::cout << "[INIT] Game Mode: " << (isSimulation ? "Simulation" : "Player") << std::endl;

        wr::rendering::TileHandler tileHandler;
        if (!tileHandler.init()) return -1;

        wr::core::ThreadPool threadPool;
        entt::registry registry;

        registry.storage<entt::entity>().reserve(65000);
        registry.storage<wr::ecs::WorldPos>().reserve(65000);
        registry.storage<wr::ecs::ScreenPos>().reserve(65000);
        registry.storage<wr::ecs::LogicalPos>().reserve(65000);
        registry.storage<wr::ecs::SpriteComponent>().reserve(65000);
        registry.storage<wr::ecs::ResourceTag>().reserve(60000);
        registry.storage<wr::ecs::SolidTag>().reserve(60000);

        registry.ctx().emplace<const wr::rendering::TileHandler*>(&tileHandler);
        registry.ctx().emplace<wr::core::ThreadPool*>(&threadPool);
        registry.ctx().emplace<wr::ecs::GameMode>(gameMode);

        wr::input::CursorHandler cursor;
        cursor.init(tileHandler.getCursorTexture());
        cursor.setSystemCursorVisible(window, false);

        wr::world::WorldGenerator worldGen(12345);
        wr::world::ChunkManager chunkManager(threadPool, registry, worldGen, tileHandler);
        wr::simulation::ChunkSimulation chunkSim(threadPool, registry, chunkManager);
        wr::simulation::UnitControlSystem unitCtrl(chunkManager);
        wr::simulation::OverseerSystem overseer;

        wr::rendering::Camera camera(1280.0f, 720.0f);
        wr::rendering::RenderSystem renderSystem(5000);
        wr::rendering::ViewDirection currentViewDir = wr::rendering::ViewDirection::North;

        for (int i = 0; i < 9; ++i) {
            auto entity = registry.create();

            float spawnX = 10.0f + i * 2.0f;
            float spawnY = 10.0f + (i % 2);

            int64_t bx = static_cast<int64_t>(std::floor(spawnX));
            int64_t by = static_cast<int64_t>(std::floor(spawnY));

            wr::world::TileInfo info = worldGen.generateTile(bx, by);
            float zHeight = 0.0f;
            if (info.type == wr::core::TerrainType::Water) zHeight = -16.0f;
            else if (info.elevationLevel == 2) zHeight = 32.0f;
            else if (info.elevationLevel == 3) zHeight = 64.0f;
            else if (info.elevationLevel == 4) zHeight = 96.0f;
            else if (info.elevationLevel == 5) zHeight = 128.0f;
            else if (info.elevationLevel == 6) zHeight = 160.0f;
            else if (info.elevationLevel == 7) zHeight = 192.0f;

            registry.emplace<wr::ecs::LogicalPos>(entity, bx, by);
            registry.emplace<wr::ecs::WorldPos>(entity, spawnX, spawnY, zHeight, zHeight);
            registry.emplace<wr::ecs::ScreenPos>(entity);
            registry.emplace<wr::ecs::Velocity>(entity);
            registry.emplace<wr::ecs::UnitTag>(entity);
            auto& anim = registry.emplace<wr::ecs::AnimationState>(entity, 0.0f, 0.1f, 192, 192, 6, 0);

            auto& spr = registry.emplace<wr::ecs::SpriteComponent>(entity).sprite;
            spr.setOrigin(96.0f, 112.0f);
            spr.setTextureRect(sf::IntRect(0, 0, 192, 192));

            if (i == 0 || i == 1) {
                registry.emplace<wr::ecs::UnitData>(entity, wr::ecs::UnitType::Warrior, wr::ecs::HeldItem::None, uint8_t{0});
                registry.emplace<wr::ecs::CombatStats>(entity, wr::simulation::config::WARRIOR_DAMAGE, wr::simulation::config::WARRIOR_RANGE, wr::simulation::config::WARRIOR_ARC);
                registry.emplace<wr::ecs::WorkerState>(entity);
                registry.emplace<wr::ecs::Health>(entity, wr::simulation::config::WARRIOR_HP, wr::simulation::config::WARRIOR_HP);
                spr.setTexture(tileHandler.getWarriorIdleTexture());
            } else if (i == 2) {
                registry.emplace<wr::ecs::UnitData>(entity, wr::ecs::UnitType::Lumberjack, wr::ecs::HeldItem::None, uint8_t{0});
                registry.emplace<wr::ecs::CombatStats>(entity, wr::simulation::config::LUMBERJACK_DAMAGE, wr::simulation::config::LUMBERJACK_RANGE, wr::simulation::config::LUMBERJACK_ARC);
                registry.emplace<wr::ecs::WorkerState>(entity);
                registry.emplace<wr::ecs::Health>(entity, wr::simulation::config::LUMBERJACK_HP, wr::simulation::config::LUMBERJACK_HP);
                spr.setTexture(tileHandler.getPawnIdleAxeTexture());
            } else if (i == 3) {
                registry.emplace<wr::ecs::UnitData>(entity, wr::ecs::UnitType::Miner, wr::ecs::HeldItem::None, uint8_t{0});
                registry.emplace<wr::ecs::CombatStats>(entity, wr::simulation::config::MINER_DAMAGE, wr::simulation::config::MINER_RANGE, wr::simulation::config::MINER_ARC);
                registry.emplace<wr::ecs::WorkerState>(entity);
                registry.emplace<wr::ecs::Health>(entity, wr::simulation::config::MINER_HP, wr::simulation::config::MINER_HP);
                spr.setTexture(tileHandler.getPawnIdlePickaxeTexture());
            } else if (i == 4) {
                registry.emplace<wr::ecs::UnitData>(entity, wr::ecs::UnitType::Builder, wr::ecs::HeldItem::None, uint8_t{0});
                registry.emplace<wr::ecs::CombatStats>(entity, wr::simulation::config::BUILDER_DAMAGE, wr::simulation::config::BUILDER_RANGE, wr::simulation::config::BUILDER_ARC);
                registry.emplace<wr::ecs::WorkerState>(entity);
                registry.emplace<wr::ecs::Health>(entity, wr::simulation::config::BUILDER_HP, wr::simulation::config::BUILDER_HP);
                spr.setTexture(tileHandler.getPawnIdleHammerTexture());
            } else {
                registry.emplace<wr::ecs::UnitData>(entity, wr::ecs::UnitType::Courier, wr::ecs::HeldItem::None, uint8_t{0});
                registry.emplace<wr::ecs::CombatStats>(entity, wr::simulation::config::COURIER_DAMAGE, wr::simulation::config::COURIER_RANGE, wr::simulation::config::COURIER_ARC);
                registry.emplace<wr::ecs::WorkerState>(entity);
                registry.emplace<wr::ecs::Health>(entity, wr::simulation::config::COURIER_HP, wr::simulation::config::COURIER_HP);
                spr.setTexture(tileHandler.getPawnIdleTexture());
            }
        }

        sf::Clock clock;
        sf::Clock statsClock;
        int frames = 0;

        int cachedFPS = 0;
        size_t cachedRamMB = 0;

        while (window.isOpen()) {
            float dt = clock.restart().asSeconds();
            sf::Event ev;

            while (window.pollEvent(ev)) {
                if (ev.type == sf::Event::Closed) window.close();
                camera.handleEvent(ev);

                if (isSimulation) {
                    if (ev.type == sf::Event::MouseButtonReleased && ev.mouseButton.button == sf::Mouse::Left) {
                        wr::input::CursorClickSystem::handleMouseClick(registry, window, camera);
                    }
                } else {
                    if (ev.type == sf::Event::MouseButtonPressed && ev.mouseButton.button == sf::Mouse::Left) {
                        cursor.startDrag();
                    }
                    if (ev.type == sf::Event::MouseButtonReleased && ev.mouseButton.button == sf::Mouse::Left) {
                        bool wasValidDrag = cursor.endDrag(registry, camera, chunkManager);
                        if (!wasValidDrag) {
                            cursor.handleMouseClick(registry, window, camera, chunkManager);
                        }
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
                        registry.clear<wr::ecs::FocusedTaskTag>();
                        registry.clear<wr::ecs::FocusedAreaTag>();
                        camera.clearFollowTarget();
                    }
                }
            }

            bool isTaskMode = false;
            bool isBuilderMode = false;
            bool isRightClick = false;

            if (isSimulation) {
                wr::core::ScopedTimer overseerTimer("0_Overseer_Update");
                overseer.updateEconomy(registry);
                overseer.manageResources(registry, chunkManager);
                overseer.manageLogistics(registry, chunkManager);
                overseer.planExpansion(registry, chunkManager);
                overseer.processDemographics(registry);
            } else {
                isTaskMode = registry.ctx().contains<wr::ecs::TaskModeState>() && registry.ctx().get<wr::ecs::TaskModeState>().active;
                isBuilderMode = registry.ctx().contains<wr::ecs::BuilderModeState>() && registry.ctx().get<wr::ecs::BuilderModeState>().active;
                isRightClick = sf::Mouse::isButtonPressed(sf::Mouse::Right);
            }

            {
                wr::core::ScopedTimer logicTimer("1_UnitControl_Update");
                unitCtrl.update(registry, dt, currentViewDir, cursor.getExactHoveredWorldPos(), isRightClick, isTaskMode);
            }

            auto selectedView = registry.view<wr::ecs::SelectedTag, wr::ecs::WorldPos>();
            int selectedCount = std::distance(selectedView.begin(), selectedView.end());

            if (selectedCount == 1 && !isTaskMode && !isBuilderMode) {
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

            {
                wr::core::ScopedTimer simTimer("2_ChunkSim_Update");
                chunkManager.update(camPx, currentViewDir);
                chunkSim.update(dt, currentViewDir, camera);
            }

            {
                wr::core::ScopedTimer perspTimer("3_Perspective_Update");
                cursor.update(window, camera, chunkManager, registry, dt);
                wr::rendering::Perspective::updateScreenPositions(registry, camera, currentViewDir);
                renderSystem.updateInterpolation(registry, dt);
            }

            wr::rendering::ActiveDragData dragData;
            if (!isSimulation) {
                dragData.isDragging = cursor.isDragging();
                dragData.startWorld = cursor.getDragStartWorldPos();
                dragData.endWorld = cursor.getExactHoveredWorldPos();
                dragData.color = cursor.getDragColor();
            }

            {
                wr::core::ScopedTimer renderTimer("4_RenderSystem");
                window.clear(sf::Color(10, 10, 15));
                renderSystem.render(registry, window, camera, tileHandler, chunkManager.getChunks(), currentViewDir, dragData);
                cursor.draw(window);
            }

            {
                wr::core::ScopedTimer vsyncTimer("5_Window_Display");
                window.display();
            }

            frames++;
            if (statsClock.getElapsedTime().asSeconds() >= 1.0f) {
                cachedRamMB = wr::core::Profiler::get().getProcessMemoryMB();
                cachedFPS = frames;
                double cpuUsage = wr::core::Profiler::get().getProcessCPUUsage();

                std::cout << "\r[STATS] FPS: " << cachedFPS
                          << " | RAM: " << cachedRamMB << "MB"
                          << " | CPU: " << std::fixed << std::setprecision(1) << cpuUsage << "%"
                          << " | Entities: " << registry.alive() << "    " << std::flush;

                wr::core::Profiler::get().logSystemStats(cachedFPS, cachedRamMB, registry.alive(), cpuUsage);

                frames = 0;
                statsClock.restart();
            }

            wr::core::Profiler::get().logFrameBoundary();
        }
    } catch (const std::exception& e) {
        std::cerr << "FATAL ERROR: " << e.what() << std::endl;
        return -1;
    }
    return 0;
}
