struct PSInput {
    float4 position: SV_Position;
    float4 color: COLOR;
    float4 normal: NORMAL;
    float4 frag_pos: POSITION;
    float2 uv: UV;
};

struct Vertex {
    float4 position;
    float4 normal;
    float2 uv;
    float2 padding;
    uint4 joints;
    float4 weights;
};

struct View {
    float4x4 projection;
    float4x4 view;
    float4 position;
};

struct Material {
    float4 color;
    uint color_texture;
    uint color_sampler;
    float3 padding;
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
    uint vertex_buffer_offset;
    uint view_buffer_index;
    uint material_buffer_index;
    uint material_buffer_offset;
    uint transform_buffer_index;
    uint transform_buffer_offset;
    uint light_buffer_index;
    uint light_count;
    uint joint_buffer_index;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> push_constants: register(b0, space0);
[[vk::binding(0, 0)]] ByteAddressBuffer bindless_buffers[]: register(t1);
[[vk::binding(0, 1)]] Texture2D<float4> bindless_textures[]: register(t2);
[[vk::binding(0, 2)]] SamplerState bindless_samplers[]: register(t3);

float4x4 get_skin_matrix(Vertex vertex) {
    float4x4 joint0 = bindless_buffers[push_constants.joint_buffer_index].Load<float4x4>(64*vertex.joints.x);
    float4x4 joint1 = bindless_buffers[push_constants.joint_buffer_index].Load<float4x4>(64*vertex.joints.y);
    float4x4 joint2 = bindless_buffers[push_constants.joint_buffer_index].Load<float4x4>(64*vertex.joints.z);
    float4x4 joint3 = bindless_buffers[push_constants.joint_buffer_index].Load<float4x4>(64*vertex.joints.w);

    float4x4 skin_matrix = vertex.weights.x * joint0 + vertex.weights.y * joint1 + vertex.weights.z * joint2 + vertex.weights.w * joint3;
    return skin_matrix;
}

PSInput VSMain(uint vertex_id: SV_VertexId) {
    Vertex vertex = bindless_buffers[push_constants.vertex_buffer_index].Load<Vertex>(80 * (push_constants.vertex_buffer_offset + vertex_id));
    View view = bindless_buffers[push_constants.view_buffer_index].Load<View>(0);
    Material material = bindless_buffers[push_constants.material_buffer_index].Load<Material>(push_constants.material_buffer_offset * 16);
    Transform transform = bindless_buffers[push_constants.transform_buffer_index].Load<Transform>(push_constants.transform_buffer_offset * 64);

    float4x4 skin_matrix = get_skin_matrix(vertex);    

    vertex.position = mul(skin_matrix, vertex.position);
    float4 frag_pos = vertex.position;
    vertex.position = mul(view.view, vertex.position);
    vertex.position = mul(view.projection, vertex.position);

    PSInput result;
    result.position = vertex.position;
    result.color = material.color;
    result.normal = vertex.normal;
    result.frag_pos = frag_pos;
    result.uv = vertex.uv;
    return result;
}

float4 PSMain(PSInput input): SV_TARGET {
    Light light = bindless_buffers[push_constants.light_buffer_index].Load<Light>(0);
    View view = bindless_buffers[push_constants.view_buffer_index].Load<View>(0);
    Material material = bindless_buffers[push_constants.material_buffer_index].Load<Material>(push_constants.material_buffer_offset * 16);
    Texture2D<float4> color_texture = bindless_textures[material.color_texture];
    SamplerState sampler = bindless_samplers[material.color_sampler];

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

    float3 color = (ambient + diffuse + specular) * color_texture.Sample(sampler, input.uv).xyz;
    return float4(color, 1.0f);
}