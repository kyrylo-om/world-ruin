cmake_minimum_required(VERSION 3.15)
project(WorldRuin)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

include(FetchContent)

# Fetch SFML
FetchContent_Declare(
        SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG 2.6.2
)
FetchContent_MakeAvailable(SFML)

# Fetch EnTT
FetchContent_Declare(
        entt
        GIT_REPOSITORY https://github.com/skypjack/entt.git
        GIT_TAG v3.11.1
)
FetchContent_MakeAvailable(entt)

# Fetch FastNoiseLite
FetchContent_Declare(
        fastnoise
        GIT_REPOSITORY https://github.com/Auburn/FastNoiseLite.git
        GIT_TAG master
)
FetchContent_MakeAvailable(fastnoise)

set(SOURCES
        src/main.cpp
        src/Core/ThreadPool.cpp
        src/Input/CursorHandler.cpp
        src/Input/CursorRaycast.cpp
        src/Input/CursorTaskSystem.cpp
        src/Input/CursorDragSystem.cpp
        src/Input/CursorClickSystem.cpp
        src/Rendering/Camera.cpp
        src/Rendering/Perspective.cpp
        src/Rendering/RenderSystem.cpp
        src/Rendering/TileHandler.cpp
        src/Simulation/World/ChunkSimulation.cpp
        src/World/ChunkManager.cpp
        src/World/Generation/WorldGenerator.cpp
        src/World/Generation/WorldGenerator_Noise.cpp
        src/World/Generation/WorldGenerator_Chunk.cpp
        src/World/Generation/RiverGenerator.cpp
        src/World/Meshing/ChunkLODMesher.cpp
        src/World/Meshing/ChunkMesher.cpp
        src/Simulation/World/TaskSystem.cpp
        src/Simulation/Environment/DebrisSystem.cpp
        src/Simulation/Environment/ResourceUpdateSystem.cpp
        src/Simulation/Unit/UnitAnimationSystem.cpp
        src/Simulation/Unit/UnitPhysicsSystem.cpp
        src/Simulation/Unit/UnitTaskSystem.cpp
        src/Simulation/Unit/UnitCommandSystem.cpp
        src/Rendering/Renderers/Overlay/OverlayUtils.cpp
        src/Rendering/Renderers/Overlay/OverlayPolygons.cpp
        src/Rendering/Renderers/Overlay/OverlayText.cpp
        src/World/Meshing/MeshingUtils.cpp
        src/World/Meshing/MeshingPasses.cpp
        src/Rendering/Renderers/GeometryRenderer.cpp
        src/Rendering/Renderers/DebugRenderer.cpp
        src/Rendering/Renderers/EntityRenderer.cpp
        src/Rendering/Renderers/FoamRenderer.cpp
        src/Rendering/Renderers/CanopyRenderer.cpp
        src/Rendering/Renderers/EntityDebugRenderer.cpp
        src/Rendering/Utils/FastSorter.cpp
        src/Rendering/Renderers/OverlayRenderer.cpp
        src/Simulation/Environment/ResourceSystem.cpp
        src/Simulation/Unit/UnitMovementSystem.cpp
        src/Simulation/Unit/UnitCombatSystem.cpp
        src/Simulation/World/PathfindingSystem.cpp
        src/World/ChunkManager_Finalize.cpp
        src/Rendering/Renderers/HUDRenderer.cpp
        src/Rendering/Renderers/Overlay/OverlayText_NormalMode.cpp
        src/Rendering/Renderers/Overlay/OverlayText_TaskMode.cpp
        src/Rendering/Renderers/Overlay/OverlayText_World.cpp
        src/Input/CursorBuilderSystem.cpp
        src/Rendering/Renderers/Overlay/OverlayText_BuilderMode.cpp
        src/Input/CursorTaskSystem/TaskFinalizer.cpp
        src/Input/CursorTaskSystem/TaskDropoffManager.cpp
        src/Input/CursorTaskSystem/TaskAreaManager.cpp
        src/Simulation/Unit/Combat/CombatMovement.cpp
        src/Simulation/Unit/Combat/CombatAnimation.cpp

        src/Simulation/BuildingPlacement.cpp

        # --- OVERSEER CORE ---
        src/Simulation/World/Overseer/OverseerSystem.cpp
        src/Simulation/World/Overseer/OverseerExpansion.cpp
        src/Simulation/World/Overseer/OverseerDemographics.cpp

        # --- OVERSEER ECONOMY (NEW FOLDER) ---
        src/Simulation/World/Overseer/OverseerEconomy/EconomyManager.cpp
        src/Simulation/World/Overseer/OverseerEconomy/HarvestDirector.cpp
        src/Simulation/World/Overseer/OverseerEconomy/LogisticsDirector.cpp

        # --- UNIT CONTROL SYSTEM (NEW FOLDER) ---
        src/Simulation/Unit/UnitControlSystem/UnitControlSystem.cpp
        src/Simulation/Unit/UnitControlSystem/UnitCommandInterface.cpp
        src/Simulation/Unit/UnitControlSystem/UnitJobAssignment.cpp
        src/Simulation/Unit/UnitControlSystem/UnitPhysicsExecution.cpp

        src/Simulation/Unit/Combat/CombatActionResolver/CombatActionResolver.cpp
        src/Simulation/Unit/Combat/CombatActionResolver/CourierResolver.cpp
        src/Simulation/Unit/Combat/CombatActionResolver/BuilderResolver.cpp
        src/Simulation/Unit/Combat/CombatActionResolver/HarvesterResolver.cpp
)

add_executable(WorldRuin ${SOURCES})

target_include_directories(WorldRuin PRIVATE
        include
        ${fastnoise_SOURCE_DIR}/Cpp
)

target_link_libraries(WorldRuin PRIVATE
        sfml-graphics
        sfml-window
        sfml-system
        EnTT::EnTT
)