struct VSInput
{
    float3 position : POSITION0;
    float4 color : COLOR0;
    float3 normal : NORMAL0;
    float2 texCoord : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_Position;
    float4 color : COLOR0;
    float3 worldNormal : NORMAL0;
    float4 lightPosition : TEXCOORD0;
    float2 texCoord : TEXCOORD1;
};

cbuffer ConstantData : register(b0)
{
    row_major float4x4 world;
    row_major float4x4 view;
    row_major float4x4 proj;

    float4 lightDirection;
    float4 lightColorAndAmbient;
    float4 materialSettings;

    row_major float4x4 inverseWorld;

    row_major float4x4 lightView;
    row_major float4x4 lightProj;
};

Texture2D shadowMap : register(t0);

SamplerComparisonState shadowSampler :
    register(s0);

Texture2D modelTexture : register(t1);

SamplerState modelTextureSampler :
    register(s1);

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

    output.texCoord =
        input.texCoord;

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

    float4 surfaceColor =
        input.color;

    if (materialSettings.x > 0.5f)
    {
        surfaceColor *=
            modelTexture.Sample(
                modelTextureSampler,
                input.texCoord
            );
    }

    const float3 finalColor =
        surfaceColor.rgb *
        lightColorAndAmbient.rgb *
        lightingStrength;

    return float4(
        finalColor,
        surfaceColor.a
    );
}