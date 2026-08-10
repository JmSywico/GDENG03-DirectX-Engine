cmake_minimum_required(VERSION 3.22)
project(jnpf LANGUAGES CXX)

if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "jnpf. DX11 requires an x64 MSVC Windows toolchain")
endif()

include(FetchContent)
include(CTest)

find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
    add_custom_target(docs
        COMMAND "${DOXYGEN_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/Doxyfile"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Generating jnpf. API documentation"
        VERBATIM)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Match the authoritative jnpf. dependency pins. Third-party source belongs
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

set(JNPF_PRODUCTION_MODULE_DIR "${CMAKE_SOURCE_DIR}/DX3D")
set(JNPF_IMGUI_DIR "${JNPF_PRODUCTION_MODULE_DIR}/External/ImGui")

file(GLOB_RECURSE JNPF_DX11_SOURCES CONFIGURE_DEPENDS
    "${JNPF_PRODUCTION_MODULE_DIR}/Source/*.cpp"
    "${CMAKE_SOURCE_DIR}/Game/*.cpp")
list(REMOVE_ITEM JNPF_DX11_SOURCES
    "${CMAKE_SOURCE_DIR}/Game/EditorMain.cpp"
    "${CMAKE_SOURCE_DIR}/Game/RuntimeMain.cpp")
list(APPEND JNPF_DX11_SOURCES
    "${JNPF_PRODUCTION_MODULE_DIR}/Include/DX3D/Game/SceneSerializer.cpp"
    "${JNPF_IMGUI_DIR}/imgui.cpp"
    "${JNPF_IMGUI_DIR}/imgui_draw.cpp"
    "${JNPF_IMGUI_DIR}/imgui_tables.cpp"
    "${JNPF_IMGUI_DIR}/imgui_widgets.cpp"
    "${JNPF_IMGUI_DIR}/imgui_impl_dx11.cpp"
    "${JNPF_IMGUI_DIR}/imgui_impl_win32.cpp")

add_executable(jnpf ${JNPF_DX11_SOURCES})
target_include_directories(jnpf PRIVATE
    "${JNPF_PRODUCTION_MODULE_DIR}/Include"
    "${JNPF_PRODUCTION_MODULE_DIR}/Source"
    "${JNPF_IMGUI_DIR}")
target_compile_definitions(jnpf PRIVATE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(jnpf PRIVATE /W4 /permissive- /bigobj /Zc:preprocessor)
target_link_libraries(jnpf PRIVATE
	EnTT::EnTT Jolt
	d3d11 d3dcompiler dxgi user32 gdi32 shell32 comdlg32 winmm windowscodecs ole32)

add_library(jnpf_dx11_backend STATIC
    "${CMAKE_SOURCE_DIR}/engine/Source/jnpf/Graphics/DX11/DX11Context.cpp"
    "${CMAKE_SOURCE_DIR}/engine/Include/engine/Graphics/DX11/DX11Context.h"
    "${CMAKE_SOURCE_DIR}/engine/Source/jnpf/Graphics/Mesh.cpp"
    "${CMAKE_SOURCE_DIR}/engine/Include/engine/Graphics/Mesh.h"
    "${CMAKE_SOURCE_DIR}/engine/Include/engine/Graphics/VertexTypes.h"
    "${CMAKE_SOURCE_DIR}/engine/Source/jnpf/Graphics/Texture2D.cpp"
    "${CMAKE_SOURCE_DIR}/engine/Include/engine/Graphics/Texture2D.h")
target_sources(jnpf_dx11_backend PRIVATE
    "${CMAKE_SOURCE_DIR}/engine/Source/jnpf/Graphics/ShadowRenderer.cpp"
    "${CMAKE_SOURCE_DIR}/engine/Include/engine/Graphics/ShadowRenderer.h")
target_include_directories(jnpf_dx11_backend PUBLIC
    "${CMAKE_SOURCE_DIR}/engine/Include/engine")
target_compile_definitions(jnpf_dx11_backend PRIVATE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(jnpf_dx11_backend PRIVATE /W4 /permissive- /Zc:preprocessor)
target_link_libraries(jnpf_dx11_backend PUBLIC d3d11 dxgi windowscodecs ole32 fmt::fmt)

# Build the canonical editor workbench independently from the proven DX11
# executable. This keeps panel, command, viewport-SRV, and profiler code
# continuously compile-checked during the engine migration.
add_library(jnpf_editor_ui STATIC
    engine/Source/jnpf/Layer/Layer.cpp
    engine/Source/jnpf/Layer/LayerStack.cpp
    engine/Source/jnpf/Layer/EditorLayer.cpp
    engine/Source/jnpf/Layer/DebugLayer.cpp
    engine/Source/jnpf/Editor/EditorContext.cpp
    engine/Source/jnpf/Editor/UI/EditorLoadingWindow.cpp
    engine/Source/jnpf/Editor/UI/Chrome/TitleBar.cpp
    engine/Source/jnpf/Editor/UI/Panels/AssetBrowserPanel.cpp
    engine/Source/jnpf/Editor/UI/Panels/InspectorPanel.cpp
    engine/Source/jnpf/Editor/UI/Panels/SceneHierarchyPanel.cpp
    engine/Source/jnpf/Editor/UI/Widgets/GizmoToolbar.cpp)
target_include_directories(jnpf_editor_ui PUBLIC
    "${CMAKE_SOURCE_DIR}/engine/Include/engine"
    "${imgui_docking_SOURCE_DIR}")
target_compile_definitions(jnpf_editor_ui PRIVATE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(jnpf_editor_ui PRIVATE /W4 /permissive- /Zc:preprocessor /bigobj)
target_link_libraries(jnpf_editor_ui PUBLIC
    d3d11 EnTT::EnTT nlohmann_json::nlohmann_json fmt::fmt spdlog::spdlog psapi)

if(BUILD_TESTING)
    add_executable(jnpf_scene_tests
        Tests/SceneRegressionTests.cpp
        engine/Source/jnpf/Core/JobSystem.cpp
        engine/Source/jnpf/Scene/Scene.cpp
        engine/Source/jnpf/Scene/ComponentCatalog.cpp
        engine/Source/jnpf/Scene/SceneFactory.cpp
        engine/Source/jnpf/Scene/Serialization/SceneSerializer.cpp
        engine/Source/jnpf/Scene/Serialization/SceneMigration.cpp
        engine/Source/jnpf/Editor/Commands/EditorCommand.cpp
        engine/Source/jnpf/Editor/Commands/CommandStack.cpp
        engine/Source/jnpf/Editor/Commands/EntitySnapshot.cpp
        engine/Source/jnpf/Editor/PrefabAsset.cpp
        engine/Source/jnpf/Input/InputActions.cpp
        engine/Source/jnpf/ECS/Systems/HierarchySystem.cpp
        engine/Source/jnpf/ECS/Systems/FlyControllerSystem.cpp
        engine/Source/jnpf/ECS/Systems/RotatorSystem.cpp
        engine/Source/jnpf/ECS/Systems/TransformPropagationSystem.cpp
        engine/Source/jnpf/Graphics/Instancing/ChunkManager.cpp
        engine/Source/jnpf/Graphics/Instancing/RenderChunk.cpp
        engine/Source/jnpf/Graphics/ProcGen/ProcGen.cpp
        engine/Source/jnpf/Graphics/AssetRegistry.cpp
        engine/Source/jnpf/Graphics/ModelLoader.cpp
        engine/Source/jnpf/Graphics/MaterialAsset.cpp
        engine/Source/jnpf/Project/ProjectConfig.cpp
        engine/Source/jnpf/Physics/PhysicsWorld.cpp
        engine/Source/jnpf/Logging/Logging.cpp
        engine/Source/jnpf/Logging/Logging.win.cpp)
    target_include_directories(jnpf_scene_tests PRIVATE
        "${CMAKE_SOURCE_DIR}/engine/Include/engine")
    target_compile_definitions(jnpf_scene_tests PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_options(jnpf_scene_tests PRIVATE /W4 /permissive- /Zc:preprocessor /bigobj)
    target_link_libraries(jnpf_scene_tests PRIVATE
        jnpf_dx11_backend assimp::assimp EnTT::EnTT fmt::fmt
        spdlog::spdlog nlohmann_json::nlohmann_json Jolt xinput9_1_0)
    add_test(NAME scene_regressions COMMAND jnpf_scene_tests)
endif()

set_target_properties(jnpf PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_custom_command(TARGET jnpf POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${JNPF_PRODUCTION_MODULE_DIR}/Assets"
        "$<TARGET_FILE_DIR:jnpf>/DX3D/Assets"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:jnpf>/Scenes"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Scenes"
        "$<TARGET_FILE_DIR:jnpf>/Scenes"
    VERBATIM)

install(TARGETS jnpf RUNTIME DESTINATION . COMPONENT Runtime)
install(DIRECTORY "${JNPF_PRODUCTION_MODULE_DIR}/Assets/"
    DESTINATION DX3D/Assets COMPONENT Runtime)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/Scenes/"
    DESTINATION Scenes COMPONENT Runtime)

# Canonical jnpf. modules are being ported under engine/{Include,Source}.
# This keeps the proven DX11 executable buildable throughout that migration.
