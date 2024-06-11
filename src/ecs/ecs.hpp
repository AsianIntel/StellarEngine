#pragma once

#define FLECS_CUSTOM_BUILD
#define FLECS_CPP
#define FLECS_SYSTEM
#define FLECS_PIPELINE
#define FLECS_LOG
#include <flecs.h>

template<>
struct std::hash<flecs::entity> {
    size_t operator()(const flecs::entity& entity) const noexcept {
        return entity;
    }
};