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
    float4 lightPosition : TEXCOORD0;
    float3 objectPosition : TEXCOORD1;
};

cbuffer ConstantData : register(b0)
{
    row_major float4x4 world;
    row_major float4x4 view;
    row_major float4x4 proj;

    float4 lightDirection;
    float4 lightColorAndAmbient;
    float4 materialAlbedo;
    float4 materialEmissiveAndStrength;
    float4 materialParameters;

    row_major float4x4 inverseWorld;

    row_major float4x4 lightView;
    row_major float4x4 lightProj;
};

Texture2D shadowMap : register(t0);
Texture2D albedoMap : register(t1);

SamplerComparisonState shadowSampler :
    register(s0);
SamplerState albedoSampler : register(s1);

VSOutput VSMain(
    VSInput input
)
{
    VSOutput output;

    float4 worldPosition =
        mul(
            float4(
                input.position,
                1.0f
            ),
            world
        );

    output.position =
        mul(
            worldPosition,
            view
        );

    output.position =
        mul(
            output.position,
            proj
        );

    output.lightPosition =
        mul(
            worldPosition,
            lightView
        );

    output.lightPosition =
        mul(
            output.lightPosition,
            lightProj
        );

    output.color =
        input.color;

    output.worldNormal =
        normalize(
            mul(
                float4(
                    input.normal,
                    0.0f
                ),
                transpose(
                    inverseWorld
                )
            ).xyz
        );

    output.objectPosition = input.position;

    return output;
}

float calculateShadow(
    float4 lightPosition,
    float3 normal,
    float3 directionToLight
)
{
    if (lightPosition.w <= 0.0f)
        return 1.0f;

    float3 projectedPosition =
        lightPosition.xyz /
        lightPosition.w;

    float2 shadowCoordinates;

    shadowCoordinates.x =
        projectedPosition.x *
        0.5f +
        0.5f;

    shadowCoordinates.y =
        -projectedPosition.y *
        0.5f +
        0.5f;

    if (
        shadowCoordinates.x < 0.0f ||
        shadowCoordinates.x > 1.0f ||
        shadowCoordinates.y < 0.0f ||
        shadowCoordinates.y > 1.0f ||
        projectedPosition.z < 0.0f ||
        projectedPosition.z > 1.0f
    )
    {
        return 1.0f;
    }

    const float normalLightAmount =
        saturate(
            dot(
                normal,
                directionToLight
            )
        );

    const float shadowBias =
        max(
            0.0030f *
            (
                1.0f -
                normalLightAmount
            ),
            0.0008f
        );

    uint shadowWidth;
    uint shadowHeight;

    shadowMap.GetDimensions(
        shadowWidth,
        shadowHeight
    );

    const float2 texelSize =
        1.0f /
        float2(
            shadowWidth,
            shadowHeight
        );

    float shadowAmount = 0.0f;

    [unroll]
    for (
        int y = -1;
        y <= 1;
        ++y
    )
    {
        [unroll]
        for (
            int x = -1;
            x <= 1;
            ++x
        )
        {
            const float2 sampleOffset =
                float2(
                    x,
                    y
                ) *
                texelSize;

            shadowAmount +=
                shadowMap.SampleCmpLevelZero(
                    shadowSampler,
                    shadowCoordinates +
                        sampleOffset,
                    projectedPosition.z -
                        shadowBias
                );
        }
    }

    return shadowAmount / 9.0f;
}

float4 PSMain(
    VSOutput input
) : SV_Target
{
    const float3 normal =
        normalize(
            input.worldNormal
        );

    const float3 directionToLight =
        normalize(
            lightDirection.xyz
        );

    const float diffuseAmount =
        saturate(
            dot(
                normal,
                directionToLight
            )
        );

    const float ambientStrength =
        saturate(
            lightColorAndAmbient.a
        );

    float shadowAmount = 1.0f;

    if (lightDirection.w > 0.5f)
    {
        shadowAmount =
            calculateShadow(
                input.lightPosition,
                normal,
                directionToLight
            );
    }

    const float directLighting =
        (
            1.0f -
            ambientStrength
        ) *
        diffuseAmount *
        shadowAmount;

    const float lightingStrength =
        ambientStrength +
        directLighting;

    float3 surfaceColor = input.color.rgb * materialAlbedo.rgb;
    if (materialParameters.y > 0.5f)
    {
        const float3 blend = abs(normal);
        const float2 uv = blend.y >= blend.x && blend.y >= blend.z
            ? input.objectPosition.xz + 0.5f
            : (blend.x >= blend.z ? input.objectPosition.zy + 0.5f : input.objectPosition.xy + 0.5f);
        surfaceColor *= albedoMap.Sample(albedoSampler, uv).rgb;
    }
    float appliedLighting = lightingStrength;

    const int materialMode = (int)(materialParameters.x + 0.5f);
    if (materialMode == 1)
    {
        surfaceColor = saturate(abs(normal.zxy) * 1.25f);
        appliedLighting = 1.0f;
    }
    else if (materialMode == 2)
    {
        surfaceColor = float3(1.0f, 0.08f, 0.06f);
        appliedLighting = 1.0f;
    }
    else if (materialMode == 3)
    {
        surfaceColor = float3(0.08f, 1.0f, 0.18f);
        appliedLighting = 1.0f;
    }
    else if (materialMode == 4)
    {
        surfaceColor = float3(0.08f, 0.22f, 1.0f);
        appliedLighting = 1.0f;
    }

    const float3 finalColor =
        surfaceColor * lightColorAndAmbient.rgb * appliedLighting +
        materialEmissiveAndStrength.rgb * materialEmissiveAndStrength.a;

    return float4(
        finalColor,
        input.color.a * materialAlbedo.a
    );
}
