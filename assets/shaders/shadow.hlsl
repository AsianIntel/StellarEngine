struct PSInput {
    float4 position: SV_Position;
};

struct Vertex {
    float4 position;
    float4 normal;
    float2 uv;
    float2 padding;
    uint4 joints;
    float4 weights;
};

struct Light {
    float4 color;
	float4 position;
    float4x4 view_projection;
    uint depth_texture;
    float3 padding;
};

struct Transform {
    float4x4 transform;
};

struct PushConstants {
    uint vertex_buffer_index;
    uint vertex_buffer_offset;
    uint transform_buffer_index;
    uint transform_buffer_offset;
    uint light_buffer_index;
    uint light_buffer_offset;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> push_constants: register(b0, space0);
[[vk::binding(0, 0)]] ByteAddressBuffer bindless_buffers[]: register(t1);
[[vk::binding(0, 1)]] Texture2D<float4> bindless_textures[]: register(t2);
[[vk::binding(0, 2)]] SamplerState bindless_samplers[]: register(t3);

PSInput VSMain(uint vertex_id: SV_VertexId) {
    Vertex vertex = bindless_buffers[push_constants.vertex_buffer_index].Load<Vertex>(80 * (push_constants.vertex_buffer_offset + vertex_id));
    Transform transform = bindless_buffers[push_constants.transform_buffer_index].Load<Transform>(push_constants.transform_buffer_offset * 64);
    Light light = bindless_buffers[push_constants.light_buffer_index].Load<Light>(push_constants.light_buffer_offset * 112);

#ifndef MESH_SKINNING
    vertex.position = mul(transform.transform, vertex.position);
#endif

    vertex.position = mul(light.view_projection, vertex.position);

    PSInput result;
    result.position = vertex.position;
    return result;
}