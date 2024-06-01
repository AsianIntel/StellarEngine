cmake_minimum_required(VERSION 3.26)
project(StellarEngine)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

FetchContent_Declare(
	glm
	GIT_REPOSITORY https://github.com/g-truc/glm
	GIT_TAG bf71a834948186f4097caa076cd2663c69a10e1e
)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(
	flecs
	GIT_REPOSITORY https://github.com/SanderMertens/flecs/
	GIT_TAG v4.0.0-beta
)
FetchContent_MakeAvailable(flecs)

FetchContent_Declare(
    VMA
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
    GIT_TAG eaf8fc27eeadf6f21b11183651b829e897f01957
)
FetchContent_MakeAvailable(VMA)

FetchContent_Declare(
    fastgltf
    GIT_REPOSITORY https://github.com/spnda/fastgltf
    GIT_TAG b73f25b68b38d0ca04a6150cadd64e873a0367ca
)
FetchContent_MakeAvailable(fastgltf)

find_package(Vulkan REQUIRED)

add_executable(StellarEngine)
target_sources(StellarEngine PUBLIC 
    "src/main.cpp"
    "src/core/app.hpp"
    "src/ecs/ecs.hpp"
    "src/render/vulkan/core.cpp"
)
target_sources(StellarEngine PUBLIC FILE_SET all_modules TYPE CXX_MODULES FILES
	"src/core/core.ixx"
    "src/core/result.ixx"
    "src/render/primitives.ixx"
    "src/render/vulkan/core.ixx"
    "src/render/vulkan/types.ixx"
    "src/render/vulkan/plugin.ixx"
    "src/render/vulkan/shader_compiler.ixx"
    "src/window/window.ixx"
    "src/assets/gltf_loader.ixx"
    "src/animation/animation.ixx"
    "src/scene/transform.ixx"
)
target_link_libraries(StellarEngine PRIVATE Vulkan::Vulkan glm flecs::flecs_static GPUOpen::VulkanMemoryAllocator fastgltf dxcompiler.lib)
target_include_directories(StellarEngine PRIVATE "src")

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()