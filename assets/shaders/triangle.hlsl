struct PSInput {
    float4 position: SV_Position;
    float4 color: COLOR;
};

struct Vertex {
    float4 position;
    float4 normal;
    float2 uv;
    float2 padding;
};

struct View {
    float4x4 projection;
    float4x4 view;
};

struct Material {
    float4 color;
};

struct Transform {
    float4x4 transform;
};

struct PushConstants {
    uint vertex_buffer_index;
    uint view_buffer_index;
    uint vertex_buffer_offset;
    uint material_buffer_index;
    uint material_buffer_offset;
    uint transform_buffer_index;
    uint transform_buffer_offset;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> push_constants: register(b0, space0);
[[vk::binding(0, 0)]] ByteAddressBuffer bindless_buffers[]: register(t1);

PSInput VSMain(uint vertex_id: SV_VertexId) {
    Vertex vertex = bindless_buffers[push_constants.vertex_buffer_index].Load<Vertex>(48 * (push_constants.vertex_buffer_offset + vertex_id));
    View view = bindless_buffers[push_constants.view_buffer_index].Load<View>(0);
    Material material = bindless_buffers[push_constants.material_buffer_index].Load<Material>(push_constants.material_buffer_offset * 16);
    Transform transform = bindless_buffers[push_constants.transform_buffer_index].Load<Transform>(push_constants.transform_buffer_offset * 64);

    vertex.position = mul(transform.transform, vertex.position);
    vertex.position = mul(view.view, vertex.position);
    vertex.position = mul(view.projection, vertex.position);

    PSInput result;
    result.position = vertex.position;
    result.color = material.color;
    return result;
}

float4 PSMain(PSInput input): SV_TARGET {
    return input.color;
}