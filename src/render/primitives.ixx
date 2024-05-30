module;

#include <glm/vec4.hpp>
#include <glm/vec2.hpp>
#include <vector>
#include <optional>

export module stellar.render.primitives;

export struct Vertex {
    glm::vec4 position;
    glm::vec4 normal;
    glm::vec2 uv;
    glm::vec2 padding{};
    glm::uvec4 joints;
    glm::vec4 weights;
};

export struct Mesh {
    std::vector<Vertex> vertices;
    std::optional<std::vector<uint32_t>> indices;
};

export Mesh cube(const float half_size) {
    float min = -half_size;
    float max = half_size;

    const std::vector vertices {
        // Front
        Vertex { {min, min, max, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 0.0f, 0.0f } },
        Vertex {  { max, min, max, 1.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },
        Vertex { { max, max, max, 1.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f } },
        Vertex {  { min, max, max, 1.0f },  { 0.0f, 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f } },
        // Back
        Vertex { { min, max, min, 1.0f },  { 0.0f, 0.0f, -1.0f, 0.0f },{ 1.0f, 0.f } },
        Vertex { { max, max, min, 1.0f },  { 0.0f, 0.0f, -1.0f, 0.0f },{ 0.0f, 0.f } },
        Vertex { { max, min, min, 1.0f },  { 0.0f, 0.0f, -1.0f, 0.0f },{ 0.0f, 1.f } },
        Vertex { { min, min, min, 1.0f },  { 0.0f, 0.0f, -1.0f, 0.0f },{ 1.0f, 1.f } },
        // Right
        Vertex { { max, min, min, 1.0f },  { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f }},
        Vertex { { max, max, min, 1.0f },  { 1.0f, 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f }},
        Vertex { { max, max, max, 1.0f },  { 1.0f, 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f }},
        Vertex { { max, min, max, 1.0f },  { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f }},
        // Left
        Vertex { { min, min, max, 1.0f },  { -1.0f, 0.0f, 0.0f, 0.0f },{ 1.0f, 0.f } },
        Vertex { { min, max, max, 1.0f },  { -1.0f, 0.0f, 0.0f, 0.0f },{ 0.0f, 0.f } },
        Vertex { { min, max, min, 1.0f },  { -1.0f, 0.0f, 0.0f, 0.0f },{ 0.0f, 1.f } },
        Vertex { { min, min, min, 1.0f },  { -1.0f, 0.0f, 0.0f, 0.0f },{ 1.0f, 1.f } },
        // Top
        Vertex { { max, max, min, 1.0f },  { 0.0f, 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
        Vertex { { min, max, min, 1.0f },  { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f } },
        Vertex { { min, max, max, 1.0f },  { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f } },
        Vertex { { max, max, max, 1.0f },  { 0.0f, 1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f } },
        // Bottom
        Vertex { { max, min, max, 1.0f },  { 0.0f, -1.0f, 0.0f, 0.0f },{ 0.0f, 0.0f } },
        Vertex { { min, min, max, 1.0f },  { 0.0f, -1.0f, 0.0f, 0.0f },{ 1.0f, 0.0f } },
        Vertex { { min, min, min, 1.0f },  { 0.0f, -1.0f, 0.0f, 0.0f },{ 1.0f, 1.0f } },
        Vertex { { max, min, min, 1.0f },  { 0.0f, -1.0f, 0.0f, 0.0f },{ 0.0f, 1.0f } },
    };

    const std::vector<uint32_t> indices {
        0, 1, 2, 2, 3, 0, // front
        4, 5, 6, 6, 7, 4, // back
        8, 9, 10, 10, 11, 8, // right
        12, 13, 14, 14, 15, 12, // left
        16, 17, 18, 18, 19, 16, // top
        20, 21, 22, 22, 23, 20, // bottom
    };

    return Mesh { .vertices = vertices, .indices = indices };
}