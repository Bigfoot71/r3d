cmake_minimum_required(VERSION 3.8)
project(r3d VERSION 0.8.0 LANGUAGES C CXX)

# ========================================
# Standards and Policies
# ========================================

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    # This section configures MSVC (Visual Studio) to enable "Edit and Continue" debugging
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# ========================================
# Project Configuration
# ========================================

set(R3D_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(R3D_IS_MAIN ON)
else()
    set(R3D_IS_MAIN OFF)
endif()

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${R3D_ROOT_PATH}/cmake")

option(R3D_RAYLIB_VENDORED "Use vendored raylib from submodule" OFF)
option(R3D_ASSIMP_VENDORED "Use vendored assimp from submodule" OFF)
option(R3D_SUPPORT_ASSIMP "Build with Assimp importer support" ON)
option(R3D_BUILD_EXAMPLES "Build the examples" ${R3D_IS_MAIN})
option(R3D_BUILD_DOCS "Build the doxygen documentation" OFF)

# ========================================
# Output Directories
# ========================================

set(R3D_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(R3D_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(R3D_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${R3D_RUNTIME_OUTPUT_DIRECTORY})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${R3D_LIBRARY_OUTPUT_DIRECTORY})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${R3D_ARCHIVE_OUTPUT_DIRECTORY})

# ========================================
# R3D Compile-Time Configuration
# ========================================

set(R3D_CONFIG_DEFAULTS
    R3D_MAX_LIGHT_FORWARD_PER_MESH      8
    R3D_MAX_PROBE_ON_SCREEN             8
    R3D_PROBE_CAPTURE_SIZE              256
    R3D_SHADOW_MAP_DIRECTIONAL_SIZE     4096
    R3D_SHADOW_MAP_SPOT_SIZE            2048
    R3D_SHADOW_MAP_OMNI_SIZE            2048
    R3D_CUBEMAP_IRRADIANCE_SIZE         32
    R3D_CUBEMAP_PREFILTER_SIZE          128
    R3D_MAX_SHADER_CODE_LENGTH          16384
    R3D_MAX_SHADER_SAMPLERS             4
    R3D_MAX_SHADER_UNIFORMS             16
    R3D_MAX_SCREEN_SHADERS              8
    R3D_ENABLE_TRACELOG                 1
)

list(LENGTH R3D_CONFIG_DEFAULTS _len)
math(EXPR _count "${_len} / 2")
math(EXPR _last "${_count} - 1")

foreach(i RANGE 0 ${_last})
    math(EXPR key_index "2 * ${i}")
    math(EXPR val_index "2 * ${i} + 1")
    list(GET R3D_CONFIG_DEFAULTS ${key_index} key)
    list(GET R3D_CONFIG_DEFAULTS ${val_index} val)
    set(${key} ${val} CACHE STRING "R3D compile-time configuration")
endforeach()

set(R3D_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/include")

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/r3d_config.h.in
    ${R3D_GENERATED_INCLUDE_DIR}/r3d_config.h
    @ONLY
)

# ========================================
# Dependencies: Assimp
# ========================================

if(R3D_SUPPORT_ASSIMP)
    if(R3D_ASSIMP_VENDORED)
        set(R3D_ASSIMP_SUBMODULE_PATH "${R3D_ROOT_PATH}/external/assimp")
        set(R3D_ASSIMP_CMAKELISTS "${R3D_ASSIMP_SUBMODULE_PATH}/CMakeLists.txt")

        if(NOT EXISTS "${R3D_ASSIMP_CMAKELISTS}")
            message(FATAL_ERROR 
                "Vendored assimp not found!\n"
                "Missing: ${R3D_ASSIMP_CMAKELISTS}\n"
                "Please initialize the submodule with:\n"
                "    git submodule update --init --recursive")
        endif()

        message(STATUS "Using vendored assimp from: ${R3D_ASSIMP_SUBMODULE_PATH}")

        # Common settings
        set(ASSIMP_INJECT_DEBUG_POSTFIX     OFF CACHE BOOL "")
        set(ASSIMP_WARNINGS_AS_ERRORS       OFF CACHE BOOL "")
        set(ASSIMP_IGNORE_GIT_HASH          ON CACHE BOOL "")
        set(ASSIMP_BUILD_TESTS              OFF CACHE BOOL "")
        set(ASSIMP_NO_EXPORT                ON CACHE BOOL "")
        set(ASSIMP_INSTALL                  OFF CACHE BOOL "")

        # Enabled importers
        # Note: There may be conflicts between Assimp and raylib if both enable the M3D format
        set(_ASSIMP_ENABLED_IMPORTERS
            COLLADA GLTF OBJ FBX IQM
        )

        # Disabled importers
        set(_ASSIMP_DISABLED_IMPORTERS
            AMF BVH OFF COB STL 3DS AC ASE ASSBIN B3D DXF CSM HMP 
            IRRMESH IRR LWO LWS M3D MD2 MD3 MD5 MDC MDL NFF NDO OGRE 
            OPENGEX PLY MS3D BLEND IFC XGL Q3D Q3BSP RAW SIB SMD 
            TERRAGEN 3D X X3D 3MF MMD
        )

        foreach(importer ${_ASSIMP_ENABLED_IMPORTERS})
            set(ASSIMP_BUILD_${importer}_IMPORTER ON CACHE BOOL "")
        endforeach()

        foreach(importer ${_ASSIMP_DISABLED_IMPORTERS})
            set(ASSIMP_BUILD_${importer}_IMPORTER OFF CACHE BOOL "")
        endforeach()

        add_subdirectory("${R3D_ASSIMP_SUBMODULE_PATH}")

        set(R3D_ASSIMP_INC "${R3D_ASSIMP_SUBMODULE_PATH}/include" CACHE STRING "" FORCE)
        set(R3D_ASSIMP_LIB "assimp" CACHE STRING "" FORCE)

        set_target_properties(assimp PROPERTIES 
            RUNTIME_OUTPUT_DIRECTORY ${R3D_RUNTIME_OUTPUT_DIRECTORY}
            LIBRARY_OUTPUT_DIRECTORY ${R3D_LIBRARY_OUTPUT_DIRECTORY}
            ARCHIVE_OUTPUT_DIRECTORY ${R3D_ARCHIVE_OUTPUT_DIRECTORY}
        )
    else()
        find_package(assimp QUIET)
        if(assimp_FOUND)
            message(STATUS "Using system-installed Assimp")
            set(R3D_ASSIMP_INC "${ASSIMP_INCLUDE_DIRS}" CACHE STRING "" FORCE)
            set(R3D_ASSIMP_LIB "${ASSIMP_LIBRARIES}" CACHE STRING "" FORCE)
        else()
            message(FATAL_ERROR
                "System Assimp not found.\n"
                "Please install Assimp development files for your platform, "
                "or use the vendored version included with this project.")
        endif()
    endif()
endif()

# ========================================
# Dependencies: Raylib
# ========================================

if(R3D_RAYLIB_VENDORED)
    set(R3D_RAYLIB_SUBMODULE_PATH "${R3D_ROOT_PATH}/external/raylib")
    set(R3D_RAYLIB_CMAKELISTS "${R3D_RAYLIB_SUBMODULE_PATH}/CMakeLists.txt")

    if(NOT EXISTS "${R3D_RAYLIB_CMAKELISTS}")
        message(FATAL_ERROR 
            "Vendored raylib not found!\n"
            "Missing: ${R3D_RAYLIB_CMAKELISTS}\n"
            "Please initialize the submodule with:\n"
            "    git submodule update --init --recursive")
    endif()

    message(STATUS "Using vendored raylib from: ${R3D_RAYLIB_SUBMODULE_PATH}")

    set(CUSTOMIZE_BUILD ON CACHE BOOL "" FORCE)

    # Enabled file formats
    set(_RAYLIB_ENABLED_FORMATS
        HDR JPG
    )

    # Disabled file formats
    # r3d provides its own model importers via Assimp
    # Note: There may be conflicts between raylib and Assimp if both enable the M3D format
    set(_RAYLIB_DISABLED_FORMATS
        OBJ MTL IQM GLTF VOX M3D
    )

    foreach(format ${_RAYLIB_ENABLED_FORMATS})
        set(SUPPORT_FILEFORMAT_${format} ON CACHE BOOL "" FORCE)
    endforeach()

    foreach(format ${_RAYLIB_DISABLED_FORMATS})
        set(SUPPORT_FILEFORMAT_${format} OFF CACHE BOOL "" FORCE)
    endforeach()

    add_subdirectory("${R3D_RAYLIB_SUBMODULE_PATH}")

    set(R3D_RAYLIB_INC "${R3D_RAYLIB_SUBMODULE_PATH}/src" CACHE STRING "" FORCE)
    set(R3D_RAYLIB_LIB "raylib" CACHE STRING "" FORCE)

    set_target_properties(raylib PROPERTIES 
        RUNTIME_OUTPUT_DIRECTORY ${R3D_RUNTIME_OUTPUT_DIRECTORY}
        LIBRARY_OUTPUT_DIRECTORY ${R3D_LIBRARY_OUTPUT_DIRECTORY}
        ARCHIVE_OUTPUT_DIRECTORY ${R3D_ARCHIVE_OUTPUT_DIRECTORY}
    )
else()
    find_package(raylib QUIET)
    if(raylib_FOUND)
        message(STATUS "Using system-installed raylib")
        set(R3D_RAYLIB_INC "${RAYLIB_INCLUDE_DIRS}" CACHE STRING "" FORCE)
        set(R3D_RAYLIB_LIB "${RAYLIB_LIBRARIES}" CACHE STRING "" FORCE)
    else()
        message(FATAL_ERROR
            "System raylib not found.\n"
            "Please install raylib development files for your platform, "
            "or use the vendored version included with this project.")
    endif()
endif()

# ========================================
# Library Definition
# ========================================

include(CheckLibraryExists)
include(EmbedShaders)
include(EmbedAssets)

if(BUILD_SHARED_LIBS)
    add_definitions(-DR3D_BUILD_SHARED)
    add_definitions(-DGLAD_API_CALL_EXPORT)
endif()

if(R3D_SUPPORT_ASSIMP)
    set(R3D_IMPORTER_SOURCES
        "${R3D_ROOT_PATH}/src/importer/r3d_importer_animation.c"
        "${R3D_ROOT_PATH}/src/importer/r3d_importer_material.c"
        "${R3D_ROOT_PATH}/src/importer/r3d_importer_skeleton.c"
        "${R3D_ROOT_PATH}/src/importer/r3d_importer_texture.c"
        "${R3D_ROOT_PATH}/src/importer/r3d_importer_mesh.c"
    )
endif()

add_library(${PROJECT_NAME}
    # Importer
    ${R3D_IMPORTER_SOURCES}
    # Common
    "${R3D_ROOT_PATH}/src/common/r3d_frustum.c"
    "${R3D_ROOT_PATH}/src/common/r3d_helper.c"
    "${R3D_ROOT_PATH}/src/common/r3d_image.c"
    "${R3D_ROOT_PATH}/src/common/r3d_pass.c"
    # Modules
    "${R3D_ROOT_PATH}/src/modules/r3d_texture.c"
    "${R3D_ROOT_PATH}/src/modules/r3d_target.c"
    "${R3D_ROOT_PATH}/src/modules/r3d_shader.c"
    "${R3D_ROOT_PATH}/src/modules/r3d_driver.c"
    "${R3D_ROOT_PATH}/src/modules/r3d_light.c"
    "${R3D_ROOT_PATH}/src/modules/r3d_render.c"
    "${R3D_ROOT_PATH}/src/modules/r3d_env.c"
    # Core
    "${R3D_ROOT_PATH}/src/r3d_animation_player.c"
    "${R3D_ROOT_PATH}/src/r3d_animation.c"
    "${R3D_ROOT_PATH}/src/r3d_core.c"
    "${R3D_ROOT_PATH}/src/r3d_cubemap.c"
    "${R3D_ROOT_PATH}/src/r3d_draw.c"
    "${R3D_ROOT_PATH}/src/r3d_decal.c"
    "${R3D_ROOT_PATH}/src/r3d_ambient_map.c"
    "${R3D_ROOT_PATH}/src/r3d_environment.c"
    "${R3D_ROOT_PATH}/src/r3d_importer.c"
    "${R3D_ROOT_PATH}/src/r3d_instance.c"
    "${R3D_ROOT_PATH}/src/r3d_kinematics.c"
    "${R3D_ROOT_PATH}/src/r3d_lighting.c"
    "${R3D_ROOT_PATH}/src/r3d_surface_shader.c"
    "${R3D_ROOT_PATH}/src/r3d_screen_shader.c"
    "${R3D_ROOT_PATH}/src/r3d_sky_shader.c"
    "${R3D_ROOT_PATH}/src/r3d_material.c"
    "${R3D_ROOT_PATH}/src/r3d_mesh.c"
    "${R3D_ROOT_PATH}/src/r3d_mesh_data.c"
    "${R3D_ROOT_PATH}/src/r3d_model.c"
    "${R3D_ROOT_PATH}/src/r3d_probe.c"
    "${R3D_ROOT_PATH}/src/r3d_skeleton.c"
    "${R3D_ROOT_PATH}/src/r3d_sky.c"
    "${R3D_ROOT_PATH}/src/r3d_utils.c"
    "${R3D_ROOT_PATH}/src/r3d_visibility.c"
    # External
    "${R3D_ROOT_PATH}/external/tinycthread/tinycthread.c"
)

# ========================================
# Embedded Resources
# ========================================

embed_shaders(${PROJECT_NAME}
    # Generic
    "${R3D_ROOT_PATH}/shaders/generic/color.frag"
    "${R3D_ROOT_PATH}/shaders/generic/screen.vert"
    "${R3D_ROOT_PATH}/shaders/generic/cubemap.vert"
    # Prepare
    "${R3D_ROOT_PATH}/shaders/prepare/atrous_wavelet.frag"
    "${R3D_ROOT_PATH}/shaders/prepare/bicubic_up.frag"
    "${R3D_ROOT_PATH}/shaders/prepare/lanczos_up.frag"
    "${R3D_ROOT_PATH}/shaders/prepare/smaa_blending_weigths.vert"
    "${R3D_ROOT_PATH}/shaders/prepare/smaa_blending_weigths.frag"
    "${R3D_ROOT_PATH}/shaders/prepare/smaa_edge_detection.vert"
    "${R3D_ROOT_PATH}/shaders/prepare/smaa_edge_detection.frag"
    "${R3D_ROOT_PATH}/shaders/prepare/blur_down.frag"
    "${R3D_ROOT_PATH}/shaders/prepare/blur_up.frag"
    "${R3D_ROOT_PATH}/shaders/prepare/ssao_in_down.frag"
    "${R3D_ROOT_PATH}/shaders/prepare/ssao.frag"
    "${R3D_ROOT_PATH}/shaders/prepare/ssao_blur.frag"
    "${R3D_ROOT_PATH}/shaders/prepare/ssil_in_down.frag"
    "${R3D_ROOT_PATH}/shaders/prepare/ssil.frag"
    "${R3D_ROOT_PATH}/shaders/prepare/ssgi_in_down.frag"
    "${R3D_ROOT_PATH}/shaders/prepare/ssgi.frag"
    "${R3D_ROOT_PATH}/shaders/prepare/ssr_in_down.frag"
    "${R3D_ROOT_PATH}/shaders/prepare/ssr.frag"
    "${R3D_ROOT_PATH}/shaders/prepare/dof_coc.frag"
    "${R3D_ROOT_PATH}/shaders/prepare/dof_down.frag"
    "${R3D_ROOT_PATH}/shaders/prepare/dof_blur.frag"
    "${R3D_ROOT_PATH}/shaders/prepare/bloom_down.frag"
    "${R3D_ROOT_PATH}/shaders/prepare/bloom_up.frag"
    "${R3D_ROOT_PATH}/shaders/prepare/cubemap_from_equirectangular.frag"
    "${R3D_ROOT_PATH}/shaders/prepare/cubemap_irradiance.frag"
    "${R3D_ROOT_PATH}/shaders/prepare/cubemap_prefilter.frag"
    "${R3D_ROOT_PATH}/shaders/prepare/cubemap_procedural_sky.frag"
    "${R3D_ROOT_PATH}/shaders/prepare/cubemap_custom_sky.frag"
    # Scene
    "${R3D_ROOT_PATH}/shaders/scene/scene.vert"
    "${R3D_ROOT_PATH}/shaders/scene/geometry.frag"
    "${R3D_ROOT_PATH}/shaders/scene/forward.frag"
    "${R3D_ROOT_PATH}/shaders/scene/unlit.frag"
    "${R3D_ROOT_PATH}/shaders/scene/depth.frag"
    "${R3D_ROOT_PATH}/shaders/scene/depth_cube.frag"
    "${R3D_ROOT_PATH}/shaders/scene/decal.frag"
    "${R3D_ROOT_PATH}/shaders/scene/skybox.vert"
    "${R3D_ROOT_PATH}/shaders/scene/skybox.frag"
    # Deferred
    "${R3D_ROOT_PATH}/shaders/deferred/ambient.frag"
    "${R3D_ROOT_PATH}/shaders/deferred/lighting.frag"
    "${R3D_ROOT_PATH}/shaders/deferred/compose.frag"
    # Post
    "${R3D_ROOT_PATH}/shaders/post/fog.frag"
    "${R3D_ROOT_PATH}/shaders/post/bloom.frag"
    "${R3D_ROOT_PATH}/shaders/post/dof.frag"
    "${R3D_ROOT_PATH}/shaders/post/screen.frag"
    "${R3D_ROOT_PATH}/shaders/post/output.frag"
    "${R3D_ROOT_PATH}/shaders/post/fxaa.frag"
    "${R3D_ROOT_PATH}/shaders/post/smaa.vert"
    "${R3D_ROOT_PATH}/shaders/post/smaa.frag"
    "${R3D_ROOT_PATH}/shaders/post/visualizer.frag"
)

embed_assets(${PROJECT_NAME}
    "${R3D_ROOT_PATH}/assets/brdf_lut_512_rg16_float.raw"
    "${R3D_ROOT_PATH}/assets/ssgi_lut_4096_rgba16_float.raw"
    "${R3D_ROOT_PATH}/assets/smaa_area_160x560_rg8.raw"
    "${R3D_ROOT_PATH}/assets/smaa_search_64x16_r8.raw"
)

# ========================================
# Include Directories
# ========================================

target_include_directories(${PROJECT_NAME}
    PRIVATE
        "${R3D_ROOT_PATH}/external/tinycthread"
        "${R3D_ROOT_PATH}/external/uthash"
        "${R3D_ROOT_PATH}/external/glad"
        "${R3D_GENERATED_INCLUDE_DIR}"
        "${R3D_ROOT_PATH}/include"
        "${R3D_RAYLIB_INC}"
)

if(R3D_SUPPORT_ASSIMP)
    target_link_libraries(${PROJECT_NAME} PRIVATE "${R3D_ASSIMP_LIB}")
endif()

# ========================================
# Link Libraries
# ========================================

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        "${R3D_RAYLIB_LIB}"
)

if(R3D_SUPPORT_ASSIMP)
    target_include_directories(${PROJECT_NAME} PRIVATE "${R3D_ASSIMP_INC}")
endif()

check_library_exists(m cos "" HAVE_LIB_M)
if(HAVE_LIB_M)
    target_link_libraries(${PROJECT_NAME} PUBLIC m)
endif()

# ========================================
# Compiler Settings
# ========================================

if(CMAKE_VERSION VERSION_GREATER 3.12)
    set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 11)
endif()

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /experimental:c11atomics)
endif()

# ========================================
# Documentation
# ========================================

if(R3D_BUILD_DOCS)
    find_package(Doxygen QUIET)

    if(NOT Doxygen_FOUND)
        message(FATAL_ERROR "Doxygen not found. Install Doxygen to build documentation.")
    endif()

    set(DOXYGEN_IN ${R3D_ROOT_PATH}/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
endif()

# ========================================
# Examples
# ========================================

if(R3D_BUILD_EXAMPLES)
    include("${R3D_ROOT_PATH}/examples/CMakeLists.txt")
endif()
