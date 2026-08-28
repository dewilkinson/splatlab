// Surfels.hlsl - procedural instanced point-splat renderer.
//
// Each "surfel" is a camera-facing quad billboard placed on a Fibonacci sphere.
// There are no vertex/index buffers: geometry comes entirely from SV_VertexID
// (which corner of the quad) and SV_InstanceID (which surfel). This is the
// minimal placeholder primitive a real surfel-GI splat/shading pass would
// replace or build on top of.

cbuffer SurfelsCB : register(b0)
{
    float4x4 g_ViewProj;
    float3   g_CamRight;
    float    g_Radius;
    float3   g_CamUp;
    float    g_Time;
    float3   g_SphereCenter;
    float    g_SphereRadius;
    uint     g_SurfelCount;
    float3   g_Pad;
};

struct VSOut
{
    float4 pos   : SV_POSITION;
    float2 uv    : TEXCOORD0;
    float3 color : COLOR0;
};

float3 HashColor(uint seed)
{
    seed = (seed ^ 61u) ^ (seed >> 16u);
    seed *= 9u;
    seed = seed ^ (seed >> 4u);
    seed *= 0x27d4eb2du;
    seed = seed ^ (seed >> 15u);
    float h = frac(float(seed) * 2.3283064365386963e-10);
    float3 c = saturate(abs(frac(h + float3(0.0, 1.0 / 3.0, 2.0 / 3.0)) * 6.0 - 3.0) - 1.0);
    return lerp(float3(1.0, 1.0, 1.0), c, 0.85);
}

float3 FibonacciSpherePoint(uint i, uint n)
{
    float goldenAngle = 3.14159265359 * (3.0 - sqrt(5.0));
    float t = (float)i / max((float)n, 1.0);
    float y = 1.0 - 2.0 * t;
    float r = sqrt(saturate(1.0 - y * y));
    float theta = goldenAngle * (float)i;
    return float3(cos(theta) * r, y, sin(theta) * r);
}

VSOut mainVS(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    VSOut o;

    float3 dir = FibonacciSpherePoint(instanceId, g_SurfelCount);
    float pulse = 0.02 * sin(g_Time * 2.0 + (float)instanceId);
    float3 worldPos = g_SphereCenter + dir * (g_SphereRadius + pulse);

    // Triangle-strip quad corners: 0(-1,-1) 1(1,-1) 2(-1,1) 3(1,1)
    float2 corner = float2((vertexId == 1 || vertexId == 3) ? 1.0 : -1.0,
                            (vertexId >= 2) ? 1.0 : -1.0);
    float3 offset = (g_CamRight * corner.x + g_CamUp * corner.y) * g_Radius;

    o.pos = mul(g_ViewProj, float4(worldPos + offset, 1.0));
    o.uv = corner * 0.5 + 0.5;
    o.color = HashColor(instanceId);
    return o;
}

float4 mainPS(VSOut i) : SV_Target
{
    float2 centered = i.uv * 2.0 - 1.0;
    float d = dot(centered, centered);
    if (d > 1.0)
        discard;
    float shade = saturate(1.0 - d);
    return float4(i.color * (0.35 + 0.65 * shade), 1.0);
}
