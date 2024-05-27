module;

#include <glm/ext/matrix_transform.hpp>
#include "ecs/ecs.hpp"

export module stellar.app;

import stellar.render.vulkan.plugin;
import stellar.window;
import stellar.render.primitives;

export struct App {
    flecs::world world{};

    void initialize() {
        initialize_window(world, 640, 480);
        initialize_vulkan(world);
    }

    void run() {
        while(world.progress()) {}
    }

    void shutdown() {
        destroy_vulkan(world);
    }
};
