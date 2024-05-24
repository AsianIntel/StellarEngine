cmake_minimum_required(VERSION 3.26)
project(StellarEngine)

set(CMAKE_CXX_STANDARD 23)

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

find_package(Vulkan REQUIRED)

add_executable(StellarEngine)
target_sources(StellarEngine PUBLIC 
    "src/main.cpp"
    "src/ecs/ecs.hpp"
    "src/render/vulkan/instance.cpp"
	"src/render/vulkan/adapter.cpp"
	"src/render/vulkan/device.cpp"
	"src/render/vulkan/command.cpp"
)
target_sources(StellarEngine PUBLIC FILE_SET all_modules TYPE CXX_MODULES FILES
    "src/core/app.ixx"
    "src/render/vulkan/core.ixx"
    "src/render/vulkan/types.ixx"
    "src/render/vulkan/plugin.ixx"
    "src/window/window.ixx"
)
target_link_libraries(StellarEngine PRIVATE Vulkan::Vulkan glm flecs::flecs_static GPUOpen::VulkanMemoryAllocator)
target_include_directories(StellarEngine PRIVATE "src")