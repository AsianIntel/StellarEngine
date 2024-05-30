module;

#include <vector>
#include <variant>
#include <optional>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/compatibility.hpp>
#include "ecs/ecs.hpp"

export module stellar.animation;

import stellar.scene.transform;

export enum class Interpolation {
    Linear,
    Step,
    CubicSpline
};

export struct Keyframes {
    struct Rotation {
        std::vector<glm::quat> rotations;
    };
    struct Translation {
        std::vector<glm::vec3> translations;
    };
    struct Scale {
        std::vector<glm::vec3> scales;
    };
    std::variant<Rotation, Translation, Scale> frames;
};

export struct AnimationCurve {
    std::vector<float> keyframe_timestamps;
    Keyframes keyframes;
    Interpolation interpolation;
    
    std::optional<uint32_t> find_keyframe(float seek_time) const {
        if (keyframe_timestamps[0] >= seek_time) {
            return 0;
        }
        auto it = std::lower_bound(keyframe_timestamps.begin(), keyframe_timestamps.end(), seek_time);
        if (it != keyframe_timestamps.end()) {
            auto idx = std::distance(keyframe_timestamps.begin(), it);
            return idx - 1;
        } else {
            return std::nullopt;
        }
    }
};

export struct AnimationClip {
    std::vector<std::vector<AnimationCurve>> curves;
    float duration;
};

export struct ActiveAnimation {
    float speed;
    bool playing;
    float seek_time;
};

export struct AnimationPlayer {
    flecs::entity animation;
    ActiveAnimation active_animation;
};

export struct AnimationTarget {
    uint32_t node_index;
};

void advance_animations(flecs::iter& it) {
    while (it.next()) {
        auto player = it.field<AnimationPlayer>(0);
        const AnimationClip* clip = player[0].animation.get<AnimationClip>();
        auto delta = it.delta_time();
        if (player->active_animation.playing) {
            if (player->active_animation.seek_time + (player->active_animation.speed * delta) > clip->duration) {
                player->active_animation.seek_time = 0;
            } else {
                player->active_animation.seek_time += (player->active_animation.speed * delta);
            }
        }
    }
}

void apply_animations(AnimationPlayer& player, Transform& transform, AnimationTarget& target) {
    if (!player.active_animation.playing) return;

    const AnimationClip* clip = player.animation.get<AnimationClip>();
    for (const AnimationCurve& curve: clip->curves[target.node_index]) {
        const auto keyframe_index = curve.find_keyframe(player.active_animation.seek_time).value();
        const auto previous_time = curve.keyframe_timestamps[keyframe_index];
        const auto next_time = curve.keyframe_timestamps[keyframe_index + 1];
        const auto interpolation_value = std::max((player.active_animation.seek_time - previous_time) / (next_time - previous_time), 0.0f);
        std::visit(
            [&](auto&& arg) {
                const auto node = target.node_index;
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, Keyframes::Rotation>) {
                    const auto previous_rotation = arg.rotations[keyframe_index];
                    const auto next_rotation = arg.rotations[keyframe_index + 1];
                    const auto new_rotation = glm::slerp(previous_rotation, next_rotation, interpolation_value);
                    transform.rotation = new_rotation;
                } else if constexpr (std::is_same_v<T, Keyframes::Translation>) {
                    const glm::vec3& previous_translation = arg.translations[keyframe_index];
                    const glm::vec3& next_translation = arg.translations[keyframe_index + 1];
                    const glm::vec3 new_translation = glm::lerp(previous_translation, next_translation, interpolation_value);
                    transform.translation = new_translation;
                } else if constexpr (std::is_same_v<T, Keyframes::Scale>) {
                    const auto previous_scale = arg.scales[keyframe_index];
                    const auto next_scale = arg.scales[keyframe_index + 1];
                    const auto new_scale = glm::lerp(previous_scale, next_scale, interpolation_value);
                    transform.scale = new_scale;
                }
            },
            curve.keyframes.frames
        );
    }
}

export void initialize_animation_plugin(const flecs::world& world) {
    auto advance_animations_system = world.system<AnimationPlayer>("Advance Animations")
        .term_at(0).singleton().inout(flecs::InOut)
        .write<AnimationPlayer>()
        .kind(flecs::OnUpdate)
        .run(advance_animations);

    auto apply_animations_system = world.system<AnimationPlayer, Transform, AnimationTarget>("Apply Animations")
        .term_at(0).singleton().inout(flecs::InOut)
        .write<Transform>()
        .kind(flecs::OnUpdate)
        .each(apply_animations);

    apply_animations_system.depends_on(advance_animations_system);
}