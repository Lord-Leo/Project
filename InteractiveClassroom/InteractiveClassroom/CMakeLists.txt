cmake_minimum_required(VERSION 3.20)
project(InteractiveClassroom LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# Keep project-specific MSVC flags off third-party FetchContent targets.
# Assimp/zlib already set /source-charset:utf-8; applying /utf-8 globally
# causes MSVC error D8016 because those options are incompatible together.
if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -DNOMINMAX)
endif()

find_package(OpenGL REQUIRED)

include(FetchContent)

# ---------------------------------------------------------------------------
# GLFW
# ---------------------------------------------------------------------------
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
FetchContent_MakeAvailable(glfw)

# ---------------------------------------------------------------------------
# GLM (header only)
# ---------------------------------------------------------------------------
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
)
FetchContent_MakeAvailable(glm)

# ---------------------------------------------------------------------------
# stb (header only) - stb_image.h
# ---------------------------------------------------------------------------
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    # stb_image v2.30 commit (2024-05-31); pinned to avoid floating builds.
    # Do not use GIT_SHALLOW with a raw commit hash: a depth-1 clone may not
    # contain that commit, causing stb-populate to fail during configuration.
    GIT_TAG        013ac3beddff3dbffafd5177e7972067cd2b5083
)
FetchContent_MakeAvailable(stb)


# ---------------------------------------------------------------------------
# miniaudio 0.11.25 (single-source, no system-wide install required)
# ---------------------------------------------------------------------------
set(MINIAUDIO_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(MINIAUDIO_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(MINIAUDIO_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(MINIAUDIO_INSTALL OFF CACHE BOOL "" FORCE)
set(MINIAUDIO_NO_LIBVORBIS ON CACHE BOOL "" FORCE)
set(MINIAUDIO_NO_LIBOPUS ON CACHE BOOL "" FORCE)
set(MINIAUDIO_NO_FLAC ON CACHE BOOL "" FORCE)
set(MINIAUDIO_NO_MP3 ON CACHE BOOL "" FORCE)
set(MINIAUDIO_NO_ENCODING ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    miniaudio_source
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG        0.11.25
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(miniaudio_source)

# ---------------------------------------------------------------------------
# Assimp 5.4.3 - fixed release, static build, no system-wide dependency
# ---------------------------------------------------------------------------
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)

# Force Assimp itself to a static library. The old value is restored after
# FetchContent so the parent project does not silently change other targets.
set(_INTERACTIVE_CLASSROOM_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    assimp_source
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG        v5.4.3
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(assimp_source)
set(BUILD_SHARED_LIBS ${_INTERACTIVE_CLASSROOM_BUILD_SHARED_LIBS} CACHE BOOL "" FORCE)
unset(_INTERACTIVE_CLASSROOM_BUILD_SHARED_LIBS)

if(NOT TARGET assimp::assimp)
    message(FATAL_ERROR
        "Assimp 5.4.3 FetchContent configuration failed: target assimp::assimp was not created. "
        "Check network access, Git, proxy settings, and the CMake configure log.")
endif()

# ---------------------------------------------------------------------------
# GLAD (hand written minimal OpenGL 3.3 core loader, vendored in external/glad)
# ---------------------------------------------------------------------------
add_library(glad STATIC
    external/glad/src/glad.c
)
target_include_directories(glad PUBLIC
    external/glad/include
)

# ---------------------------------------------------------------------------
# Main executable
# ---------------------------------------------------------------------------
add_executable(InteractiveClassroom
    src/main.cpp
    src/Shader.cpp
    src/Camera.cpp
    src/Mesh.cpp
    src/Classroom.cpp
    src/InteractionSystem.cpp
    src/Computer.cpp
    src/Projector.cpp
    src/Door.cpp
    src/AudioManager.cpp
    src/ClassroomAudioController.cpp
    src/Material.cpp
    src/TextureManager.cpp
    src/ModelMesh.cpp
    src/Model.cpp
    src/PostProcessor.cpp
)

# Apply warning, parallel-build, and UTF-8 settings only to our executable.
# This prevents the flags from leaking into Assimp's bundled zlib target.
if(MSVC)
    target_compile_options(InteractiveClassroom PRIVATE /MP /W3 /utf-8)
    target_compile_definitions(InteractiveClassroom PRIVATE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
    )
endif()

target_include_directories(InteractiveClassroom PRIVATE
    include
    ${glm_SOURCE_DIR}
    ${stb_SOURCE_DIR}
)

target_link_libraries(InteractiveClassroom PRIVATE
    glfw
    glad
    OpenGL::GL
    miniaudio
    assimp::assimp
)

# Copy runtime asset folders next to the built executable so relative paths
# (e.g. "shaders/classroom.vert", "textures/floor.jpg") resolve correctly
# from the Visual Studio debugger working directory, which is set below to
# the executable's own directory.
add_custom_command(TARGET InteractiveClassroom POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:InteractiveClassroom>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/textures
        $<TARGET_FILE_DIR:InteractiveClassroom>/textures
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/models
        $<TARGET_FILE_DIR:InteractiveClassroom>/models
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/sounds
        $<TARGET_FILE_DIR:InteractiveClassroom>/sounds
)

# Assimp is intentionally linked statically, so no Assimp DLL is required in
# the normal Windows package. This conditional remains as a safe guard if a
# downstream toolchain replaces the target with a shared library.
if(WIN32 AND TARGET assimp AND BUILD_SHARED_LIBS)
    add_custom_command(TARGET InteractiveClassroom POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:assimp>
            $<TARGET_FILE_DIR:InteractiveClassroom>
        VERBATIM
    )
endif()

set_target_properties(InteractiveClassroom PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:InteractiveClassroom>"
)
