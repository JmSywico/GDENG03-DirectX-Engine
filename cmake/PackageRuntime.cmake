cmake_minimum_required(VERSION 3.22)

foreach(required_variable
    PROJECT_FILE
    RUNTIME_EXECUTABLE
    VERTEX_SHADER
    FRAGMENT_SHADER
    SHADOW_VERTEX_SHADER
    SHADOW_FRAGMENT_SHADER)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "PackageRuntime requires ${required_variable}")
    endif()
endforeach()

foreach(required_file
    "${PROJECT_FILE}"
    "${RUNTIME_EXECUTABLE}"
    "${VERTEX_SHADER}"
    "${FRAGMENT_SHADER}"
    "${SHADOW_VERTEX_SHADER}"
    "${SHADOW_FRAGMENT_SHADER}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "Required package input does not exist: ${required_file}")
    endif()
endforeach()

get_filename_component(project_file "${PROJECT_FILE}" ABSOLUTE)
get_filename_component(project_root "${project_file}" DIRECTORY)
file(READ "${project_file}" project_json)

string(JSON project_version GET "${project_json}" version)
if(NOT project_version EQUAL 1)
    message(FATAL_ERROR "Unsupported project version ${project_version} in ${project_file}")
endif()
string(JSON project_name GET "${project_json}" name)
string(JSON asset_directory GET "${project_json}" assets)
string(JSON startup_scene GET "${project_json}" startupScene)
string(JSON input_actions GET "${project_json}" inputActions)
string(JSON configured_output GET "${project_json}" output)

function(resolve_portable_project_path output relative_path label)
    if("${relative_path}" STREQUAL "")
        message(FATAL_ERROR "${label} must not be empty")
    endif()
    if(IS_ABSOLUTE "${relative_path}")
        message(FATAL_ERROR "${label} must be project-relative: ${relative_path}")
    endif()
    cmake_path(ABSOLUTE_PATH relative_path
        BASE_DIRECTORY "${project_root}"
        NORMALIZE
        OUTPUT_VARIABLE absolute_path)
    file(RELATIVE_PATH path_from_root "${project_root}" "${absolute_path}")
    if(path_from_root STREQUAL ".." OR path_from_root MATCHES "^\\.\\.[/\\\\]")
        message(FATAL_ERROR "${label} escapes the project root: ${relative_path}")
    endif()
    set(${output} "${absolute_path}" PARENT_SCOPE)
endfunction()

resolve_portable_project_path(asset_source "${asset_directory}" "Asset directory")
resolve_portable_project_path(scene_source "${startup_scene}" "Startup scene")
resolve_portable_project_path(input_source "${input_actions}" "Input actions")

if(NOT IS_DIRECTORY "${asset_source}")
    message(FATAL_ERROR "Project asset directory does not exist: ${asset_source}")
endif()
if(NOT EXISTS "${scene_source}")
    message(FATAL_ERROR "Project startup scene does not exist: ${scene_source}")
endif()
if(NOT EXISTS "${input_source}")
    message(FATAL_ERROR "Project input-actions file does not exist: ${input_source}")
endif()

file(READ "${scene_source}" scene_json)
string(JSON scene_root_type ERROR_VARIABLE scene_error TYPE "${scene_json}")
if(scene_error OR NOT scene_root_type STREQUAL "OBJECT")
    message(FATAL_ERROR "Startup scene is not a valid JSON object: ${scene_source}")
endif()
string(JSON scene_header_type ERROR_VARIABLE scene_error TYPE "${scene_json}" scene)
if(scene_error OR NOT scene_header_type STREQUAL "OBJECT")
    message(FATAL_ERROR "Startup scene has no scene object: ${scene_source}")
endif()
string(JSON entities_type ERROR_VARIABLE scene_error TYPE "${scene_json}" entities)
if(scene_error OR NOT entities_type STREQUAL "ARRAY")
    message(FATAL_ERROR "Startup scene has no entities array: ${scene_source}")
endif()

string(REGEX REPLACE "[^A-Za-z0-9._-]" "_" executable_name "${project_name}")
if(executable_name STREQUAL "")
    set(executable_name "Game")
endif()

if(DEFINED PACKAGE_OUTPUT AND NOT "${PACKAGE_OUTPUT}" STREQUAL "")
    get_filename_component(package_output "${PACKAGE_OUTPUT}" ABSOLUTE
        BASE_DIR "${project_root}")
else()
    resolve_portable_project_path(output_root "${configured_output}" "Output directory")
    set(package_output "${output_root}/${executable_name}")
endif()
cmake_path(NORMAL_PATH package_output)
if(package_output STREQUAL project_root)
    message(FATAL_ERROR "Package output must not be the project root")
endif()

file(RELATIVE_PATH package_from_assets "${asset_source}" "${package_output}")
if(NOT package_from_assets STREQUAL ".."
    AND NOT package_from_assets MATCHES "^\\.\\.[/\\\\]")
    message(FATAL_ERROR "Package output must not be inside the asset directory")
endif()

set(staging_output "${package_output}.staging")
foreach(previous_output "${staging_output}" "${package_output}")
    if(EXISTS "${previous_output}")
        if(NOT EXISTS "${previous_output}/.enigne-package")
            message(FATAL_ERROR
                "Refusing to replace an unowned directory: ${previous_output}")
        endif()
        file(REMOVE_RECURSE "${previous_output}")
    endif()
endforeach()
file(MAKE_DIRECTORY "${staging_output}")
file(WRITE "${staging_output}/.enigne-package" "enignE packaged output\n")

file(COPY_FILE "${RUNTIME_EXECUTABLE}" "${staging_output}/${executable_name}.exe" ONLY_IF_DIFFERENT)
get_filename_component(project_filename "${project_file}" NAME)
file(COPY_FILE "${project_file}" "${staging_output}/${project_filename}" ONLY_IF_DIFFERENT)

get_filename_component(scene_parent "${startup_scene}" DIRECTORY)
file(MAKE_DIRECTORY "${staging_output}/${scene_parent}")
file(COPY_FILE "${scene_source}" "${staging_output}/${startup_scene}" ONLY_IF_DIFFERENT)

get_filename_component(input_parent "${input_actions}" DIRECTORY)
file(MAKE_DIRECTORY "${staging_output}/${input_parent}")
file(COPY_FILE "${input_source}" "${staging_output}/${input_actions}" ONLY_IF_DIFFERENT)

file(MAKE_DIRECTORY "${staging_output}/${asset_directory}")
file(COPY "${asset_source}/" DESTINATION "${staging_output}/${asset_directory}")

file(MAKE_DIRECTORY "${staging_output}/shaders")
foreach(shader
    "${VERTEX_SHADER}"
    "${FRAGMENT_SHADER}"
    "${SHADOW_VERTEX_SHADER}"
    "${SHADOW_FRAGMENT_SHADER}")
    file(COPY "${shader}" DESTINATION "${staging_output}/shaders")
endforeach()

file(RENAME "${staging_output}" "${package_output}")
message(STATUS "Packaged '${project_name}' to '${package_output}'")
