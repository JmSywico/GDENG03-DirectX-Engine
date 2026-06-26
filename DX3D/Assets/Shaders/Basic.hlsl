struct VSInput
{
    float3 position : POSITION0;
    float4 color : COLOR0;
    float3 normal : NORMAL0;
};

struct VSOutput
{
    float4 position : SV_Position;
    float4 color : COLOR0;
    float3 worldNormal : NORMAL0;
};

cbuffer ConstantData : register(b0)
{
    row_major float4x4 world;
    row_major float4x4 view;
    row_major float4x4 proj;

    // Direction from the surface toward the light.
    float4 lightDirection;

    // RGB = light color
    // A = ambient strength
    float4 lightColorAndAmbient;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    output.position =
        mul(float4(input.position, 1.0f), world);

    output.position =
        mul(output.position, view);

    output.position =
        mul(output.position, proj);

    output.color = input.color;

    // Convert the mesh normal from local space to world space.
    output.worldNormal = normalize(
        mul(
            float4(input.normal, 0.0f),
            world
        ).xyz
    );

    return output;
}

float4 PSMain(VSOutput input) : SV_Target
{
    const float3 normal =
        normalize(input.worldNormal);

    const float3 directionToLight =
        normalize(lightDirection.xyz);

    // Lambert diffuse lighting.
    const float diffuseAmount =
        saturate(
            dot(
                normal,
                directionToLight
            )
        );

    const float ambientStrength =
        saturate(lightColorAndAmbient.a);

    // Ambient remains visible on faces turned away from the light.
    const float lightingStrength =
        ambientStrength +
        (1.0f - ambientStrength) *
        diffuseAmount;

    const float3 finalColor =
        input.color.rgb *
        lightColorAndAmbient.rgb *
        lightingStrength;

    return float4(
        finalColor,
        input.color.a
    );
}