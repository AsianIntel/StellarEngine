struct PSInput {
    float4 position: SV_Position;
    float4 color: COLOR;
    float4 normal: NORMAL;
    float4 frag_pos: POSITION;
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
    float4 position;
};

struct Material {
    float4 color;
};

struct Transform {
    float4x4 transform;
};

struct Light {
    float4 color;
    float4 position;
};

struct PushConstants {
    uint vertex_buffer_index;
    uint view_buffer_index;
    uint vertex_buffer_offset;
    uint material_buffer_index;
    uint material_buffer_offset;
    uint transform_buffer_index;
    uint transform_buffer_offset;
    uint light_buffer_index;
    uint light_count;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> push_constants: register(b0, space0);
[[vk::binding(0, 0)]] ByteAddressBuffer bindless_buffers[]: register(t1);

PSInput VSMain(uint vertex_id: SV_VertexId) {
    Vertex vertex = bindless_buffers[push_constants.vertex_buffer_index].Load<Vertex>(48 * (push_constants.vertex_buffer_offset + vertex_id));
    View view = bindless_buffers[push_constants.view_buffer_index].Load<View>(0);
    Material material = bindless_buffers[push_constants.material_buffer_index].Load<Material>(push_constants.material_buffer_offset * 16);
    Transform transform = bindless_buffers[push_constants.transform_buffer_index].Load<Transform>(push_constants.transform_buffer_offset * 64);

    vertex.position = mul(transform.transform, vertex.position);
    float4 frag_pos = vertex.position;
    vertex.position = mul(view.view, vertex.position);
    vertex.position = mul(view.projection, vertex.position);

    PSInput result;
    result.position = vertex.position;
    result.color = material.color;
    result.normal = vertex.normal;
    result.frag_pos = frag_pos;
    return result;
}

float4 PSMain(PSInput input): SV_TARGET {
    Light light = bindless_buffers[push_constants.light_buffer_index].Load<Light>(0);
    View view = bindless_buffers[push_constants.view_buffer_index].Load<View>(0);

    float ambient_strength = 0.1f;
    float3 ambient = ambient_strength * light.color.xyz;

    float4 normal = normalize(input.normal);
    float4 light_dir = normalize(light.position - input.frag_pos);
    float diff = max(dot(normal.xyz, light_dir.xyz), 0.0f);
    float3 diffuse = diff * light.color.xyz;

    float specular_strength = 0.5f;
    float4 view_dir = normalize(view.position - input.frag_pos);
    float4 reflect_dir = reflect(-light_dir, normal);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0f), 32);
    float3 specular = specular_strength * spec * light.color.xyz;

    float3 color = (ambient + diffuse + specular) * input.color.xyz;
    return float4(color, 1.0f);
}