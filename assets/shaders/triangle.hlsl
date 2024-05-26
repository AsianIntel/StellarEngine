struct PSInput {
    float4 position: SV_Position;
    float4 color: COLOR;
};

struct Vertex {
    float4 position;
};

struct View {
    float4x4 projection;
    float4x4 view;
};

struct PushConstants {
    uint vertex_buffer_index;
    uint vertex_buffer_offset;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> push_constants: register(b0, space0);
[[vk::binding(0, 0)]] ByteAddressBuffer bindless_buffers[]: register(t1);

PSInput VSMain(uint vertex_id: SV_VertexId) {
    Vertex vertex = bindless_buffers[push_constants.vertex_buffer_index].Load<Vertex>(push_constants.vertex_buffer_offset + 16*vertex_id);

    PSInput result;
    result.position = vertex.position;
    result.color = float4(1.0f, 1.0f, 1.0f, 1.0f);
    return result;
}

float4 PSMain(PSInput input): SV_TARGET {
    return input.color;
}