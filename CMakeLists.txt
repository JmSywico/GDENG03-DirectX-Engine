cmake_minimum_required(VERSION 3.22)
project(enignE LANGUAGES CXX)

if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "enignE DX11 requires an x64 MSVC Windows toolchain")
endif()

include(FetchContent)
include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Match the authoritative enignE dependency pins. Third-party source belongs
# under the CMake build tree, never in the engine source tree.
FetchContent_Declare(
    EnTT
    URL https://github.com/skypjack/entt/archive/refs/tags/v3.14.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(EnTT)

FetchContent_Declare(
    fmt
    URL https://github.com/fmtlib/fmt/archive/refs/tags/12.1.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(fmt)

set(SPDLOG_FMT_EXTERNAL ON CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    spdlog
    URL https://github.com/gabime/spdlog/archive/refs/tags/v1.17.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/archive/refs/tags/v3.12.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(nlohmann_json)

set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    assimp
    URL https://github.com/assimp/assimp/archive/refs/tags/v6.0.4.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(assimp)

FetchContent_Declare(
    imgui_docking
    URL https://github.com/ocornut/imgui/archive/2af6dd9694288e6befe1edb7ce25510911693c22.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(imgui_docking)

set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
set(PROFILER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
set(CROSS_PLATFORM_DETERMINISTIC OFF CACHE BOOL "" FORCE)
foreach(feature SSE4_1 SSE4_2 AVX AVX2 LZCNT TZCNT F16C FMADD)
    set(USE_${feature} OFF CACHE BOOL "" FORCE)
endforeach()
FetchContent_Declare(
    JoltPhysics
    URL https://github.com/jrouwe/JoltPhysics/archive/refs/tags/v5.5.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR Build)
FetchContent_MakeAvailable(JoltPhysics)

set(ENIGNE_LEGACY_MODULE_DIR "${CMAKE_SOURCE_DIR}/DX3D")
set(ENIGNE_IMGUI_DIR "${ENIGNE_LEGACY_MODULE_DIR}/External/ImGui")

file(GLOB_RECURSE ENIGNE_DX11_SOURCES CONFIGURE_DEPENDS
    "${ENIGNE_LEGACY_MODULE_DIR}/Source/*.cpp"
    "${CMAKE_SOURCE_DIR}/Game/*.cpp")
list(REMOVE_ITEM ENIGNE_DX11_SOURCES
    "${CMAKE_SOURCE_DIR}/Game/EditorMain.cpp"
    "${CMAKE_SOURCE_DIR}/Game/RuntimeMain.cpp")
list(APPEND ENIGNE_DX11_SOURCES
    "${ENIGNE_LEGACY_MODULE_DIR}/Include/DX3D/Game/SceneSerializer.cpp"
    "${ENIGNE_IMGUI_DIR}/imgui.cpp"
    "${ENIGNE_IMGUI_DIR}/imgui_draw.cpp"
    "${ENIGNE_IMGUI_DIR}/imgui_tables.cpp"
    "${ENIGNE_IMGUI_DIR}/imgui_widgets.cpp"
    "${ENIGNE_IMGUI_DIR}/imgui_impl_dx11.cpp"
    "${ENIGNE_IMGUI_DIR}/imgui_impl_win32.cpp")

add_executable(enignE ${ENIGNE_DX11_SOURCES})
target_include_directories(enignE PRIVATE
    "${ENIGNE_LEGACY_MODULE_DIR}/Include"
    "${ENIGNE_LEGACY_MODULE_DIR}/Source"
    "${ENIGNE_IMGUI_DIR}")
target_compile_definitions(enignE PRIVATE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(enignE PRIVATE /W4 /permissive- /bigobj /Zc:preprocessor)
target_link_libraries(enignE PRIVATE
    EnTT::EnTT Jolt
    d3d11 d3dcompiler dxgi user32 gdi32 shell32 comdlg32 winmm)

add_library(enignE_dx11_backend STATIC
    "${CMAKE_SOURCE_DIR}/enignE/Source/enignE/Graphics/DX11/DX11Context.cpp"
    "${CMAKE_SOURCE_DIR}/enignE/Include/enignE/Graphics/DX11/DX11Context.h"
    "${CMAKE_SOURCE_DIR}/enignE/Source/enignE/Graphics/Mesh.cpp"
    "${CMAKE_SOURCE_DIR}/enignE/Include/enignE/Graphics/Mesh.h"
    "${CMAKE_SOURCE_DIR}/enignE/Include/enignE/Graphics/VertexTypes.h"
    "${CMAKE_SOURCE_DIR}/enignE/Source/enignE/Graphics/Texture2D.cpp"
    "${CMAKE_SOURCE_DIR}/enignE/Include/enignE/Graphics/Texture2D.h")
target_sources(enignE_dx11_backend PRIVATE
    "${CMAKE_SOURCE_DIR}/enignE/Source/enignE/Graphics/ShadowRenderer.cpp"
    "${CMAKE_SOURCE_DIR}/enignE/Include/enignE/Graphics/ShadowRenderer.h")
target_include_directories(enignE_dx11_backend PUBLIC
    "${CMAKE_SOURCE_DIR}/enignE/Include/enignE")
target_compile_definitions(enignE_dx11_backend PRIVATE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(enignE_dx11_backend PRIVATE /W4 /permissive- /Zc:preprocessor)
target_link_libraries(enignE_dx11_backend PUBLIC d3d11 dxgi windowscodecs ole32 fmt::fmt)

# Build the canonical editor workbench independently while Engine.cpp is being
# translated from bgfx to DX11. This keeps panel, command, viewport-SRV, and
# profiler code continuously compile-checked during the backend migration.
add_library(enignE_editor_ui STATIC
    enignE/Source/enignE/Layer/Layer.cpp
    enignE/Source/enignE/Layer/LayerStack.cpp
    enignE/Source/enignE/Layer/EditorLayer.cpp
    enignE/Source/enignE/Layer/DebugLayer.cpp
    enignE/Source/enignE/Editor/EditorContext.cpp
    enignE/Source/enignE/Editor/UI/EditorLoadingWindow.cpp
    enignE/Source/enignE/Editor/UI/Chrome/TitleBar.cpp
    enignE/Source/enignE/Editor/UI/Panels/AssetBrowserPanel.cpp
    enignE/Source/enignE/Editor/UI/Panels/InspectorPanel.cpp
    enignE/Source/enignE/Editor/UI/Panels/SceneHierarchyPanel.cpp
    enignE/Source/enignE/Editor/UI/Widgets/GizmoToolbar.cpp)
target_include_directories(enignE_editor_ui PUBLIC
    "${CMAKE_SOURCE_DIR}/enignE/Include/enignE"
    "${imgui_docking_SOURCE_DIR}")
target_compile_definitions(enignE_editor_ui PRIVATE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(enignE_editor_ui PRIVATE /W4 /permissive- /Zc:preprocessor /bigobj)
target_link_libraries(enignE_editor_ui PUBLIC
    d3d11 EnTT::EnTT nlohmann_json::nlohmann_json fmt::fmt spdlog::spdlog psapi)

if(BUILD_TESTING)
    add_executable(enignE_scene_tests
        Tests/SceneRegressionTests.cpp
        enignE/Source/enignE/Core/JobSystem.cpp
        enignE/Source/enignE/Scene/Scene.cpp
        enignE/Source/enignE/Scene/ComponentCatalog.cpp
        enignE/Source/enignE/Scene/SceneFactory.cpp
        enignE/Source/enignE/Scene/Serialization/SceneSerializer.cpp
        enignE/Source/enignE/Scene/Serialization/SceneMigration.cpp
        enignE/Source/enignE/Editor/Commands/EditorCommand.cpp
        enignE/Source/enignE/Editor/Commands/CommandStack.cpp
        enignE/Source/enignE/Editor/Commands/EntitySnapshot.cpp
        enignE/Source/enignE/Editor/PrefabAsset.cpp
        enignE/Source/enignE/Input/InputActions.cpp
        enignE/Source/enignE/ECS/Systems/HierarchySystem.cpp
        enignE/Source/enignE/ECS/Systems/FlyControllerSystem.cpp
        enignE/Source/enignE/ECS/Systems/RotatorSystem.cpp
        enignE/Source/enignE/ECS/Systems/TransformPropagationSystem.cpp
        enignE/Source/enignE/Graphics/Instancing/ChunkManager.cpp
        enignE/Source/enignE/Graphics/Instancing/RenderChunk.cpp
        enignE/Source/enignE/Graphics/ProcGen/ProcGen.cpp
        enignE/Source/enignE/Graphics/AssetRegistry.cpp
        enignE/Source/enignE/Graphics/ModelLoader.cpp
        enignE/Source/enignE/Graphics/MaterialAsset.cpp
        enignE/Source/enignE/Project/ProjectConfig.cpp
        enignE/Source/enignE/Physics/PhysicsWorld.cpp
        enignE/Source/enignE/Logging/Logging.cpp
        enignE/Source/enignE/Logging/Logging.win.cpp)
    target_include_directories(enignE_scene_tests PRIVATE
        "${CMAKE_SOURCE_DIR}/enignE/Include/enignE")
    target_compile_definitions(enignE_scene_tests PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_options(enignE_scene_tests PRIVATE /W4 /permissive- /Zc:preprocessor /bigobj)
    target_link_libraries(enignE_scene_tests PRIVATE
        enignE_dx11_backend assimp::assimp EnTT::EnTT fmt::fmt
        spdlog::spdlog nlohmann_json::nlohmann_json Jolt xinput9_1_0)
    add_test(NAME scene_regressions COMMAND enignE_scene_tests)
endif()

set_target_properties(enignE PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_custom_command(TARGET enignE POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${ENIGNE_LEGACY_MODULE_DIR}/Assets"
        "$<TARGET_FILE_DIR:enignE>/DX3D/Assets"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:enignE>/Scenes"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Scenes"
        "$<TARGET_FILE_DIR:enignE>/Scenes"
    VERBATIM)

install(TARGETS enignE RUNTIME DESTINATION . COMPONENT Runtime)
install(DIRECTORY "${ENIGNE_LEGACY_MODULE_DIR}/Assets/"
    DESTINATION DX3D/Assets COMPONENT Runtime)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/Scenes/"
    DESTINATION Scenes COMPONENT Runtime)

# Canonical enignE modules are being ported under enignE/{Include,Source}.
# This keeps the proven DX11 executable buildable throughout that migration.
