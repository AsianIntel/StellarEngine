struct Vertex {
    float4 position;
    float4 normal;
    float2 uv;
    float2 padding;
    uint4 joints;
    float4 weights;
};

struct PushConstants {
    uint vertex_count;
    uint vertex_buffer_index;
    uint vertex_buffer_offset;
    uint joint_buffer_index;
    uint joint_buffer_offset;
    uint output_buffer_index;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> push_constants: register(b0, space0);
[[vk::binding(0, 0)]] RWByteAddressBuffer bindless_buffers[]: register(u1);
[[vk::binding(0, 1)]] Texture2D<float4> bindless_textures[]: register(t2);
[[vk::binding(0, 2)]] SamplerState bindless_samplers[]: register(t3);

float4x4 get_skin_matrix(Vertex vertex) {
    float4x4 joint0 = bindless_buffers[push_constants.joint_buffer_index].Load<float4x4>(64 * (push_constants.joint_buffer_offset + vertex.joints.x));
    float4x4 joint1 = bindless_buffers[push_constants.joint_buffer_index].Load<float4x4>(64 * (push_constants.joint_buffer_offset + vertex.joints.y));
    float4x4 joint2 = bindless_buffers[push_constants.joint_buffer_index].Load<float4x4>(64 * (push_constants.joint_buffer_offset + vertex.joints.z));
    float4x4 joint3 = bindless_buffers[push_constants.joint_buffer_index].Load<float4x4>(64 * (push_constants.joint_buffer_offset + vertex.joints.w));

    float4x4 skin_matrix = vertex.weights.x * joint0 + vertex.weights.y * joint1 + vertex.weights.z * joint2 + vertex.weights.w * joint3;
    return skin_matrix;
}

[numthreads(128, 1, 1)]
void cs_skinning(uint3 thread_id: SV_DispatchThreadID) {
    uint vertex_id = thread_id.x;
    if (vertex_id >= push_constants.vertex_count) return;

    Vertex vertex = bindless_buffers[push_constants.vertex_buffer_index].Load<Vertex>(80 * (push_constants.vertex_buffer_offset + vertex_id));

    float4x4 skin_matrix = get_skin_matrix(vertex);
    vertex.position = mul(skin_matrix, vertex.position);

    bindless_buffers[push_constants.output_buffer_index].Store<Vertex>(80 * (push_constants.vertex_buffer_offset + vertex_id), vertex);
}