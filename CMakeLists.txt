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

# NEW: Fetch FastNoiseLite (Enterprise standard for procedural noise)
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
        src/Input/CursorTaskSystem.cpp  # NEW
        src/Input/CursorDragSystem.cpp  # NEW
        src/Input/CursorClickSystem.cpp # NEW
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
        src/World/Meshing/MeshingUtils.cpp
        src/World/Meshing/MeshingPasses.cpp
        src/Rendering/Renderers/GeometryRenderer.cpp
        src/Rendering/Renderers/DebugRenderer.cpp
        src/Simulation/Unit/UnitControlSystem.cpp
        src/Rendering/Renderers/EntityRenderer.cpp
        src/Rendering/Renderers/FoamRenderer.cpp
        src/Rendering/Renderers/CanopyRenderer.cpp
        src/Rendering/Renderers/EntityDebugRenderer.cpp
        src/Rendering/Utils/FastSorter.cpp
        src/Rendering/Renderers/OverlayRenderer.cpp
        src/Simulation/Environment/ResourceSystem.cpp
        src/Simulation/Unit/UnitMovementSystem.cpp
        src/Simulation/Unit/UnitCombatSystem.cpp
        src/Simulation/Unit/UnitPhysicsSystem.cpp
        src/Simulation/World/PathfindingSystem.cpp
        src/World/ChunkManager_Finalize.cpp
        src/Rendering/Renderers/HUDRenderer.cpp
)

add_executable(WorldRuin ${SOURCES})

# Include directories: Add fastnoise_SOURCE_DIR to resolve the header issue
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