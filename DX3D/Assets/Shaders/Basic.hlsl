/*MIT License

C++ 3D Game Tutorial Series (https://github.com/PardCode/CPP-3D-Game-Tutorial-Series)

Copyright (c) 2019-2026, PardCode

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.*/

struct VSInput
{
    float3 position : POSITION0;
    float4 color : COLOR0;
};

struct VSOutput
{
    float4 position : SV_Position;
    float4 color : COLOR0;
<<<<<<< Updated upstream
=======
    float3 worldNormal : NORMAL0;
    float4 lightPosition : TEXCOORD0;
>>>>>>> Stashed changes
};

cbuffer ConstantData : register(b0)
{
    row_major float4x4 world;
    row_major float4x4 view;
    row_major float4x4 proj;
<<<<<<< Updated upstream
=======

    float4 lightDirection;
    float4 lightColorAndAmbient;

    row_major float4x4 inverseWorld;

    row_major float4x4 lightView;
    row_major float4x4 lightProj;
>>>>>>> Stashed changes
};

Texture2D shadowMap : register(t0);

SamplerComparisonState shadowSampler : register(s0);

VSOutput VSMain(VSInput input)
{
    VSOutput output;
<<<<<<< Updated upstream
    output.position = mul(float4(input.position, 1), world);
    output.position = mul(output.position, view);
    output.position = mul(output.position, proj);
    output.color = input.color;
=======

    float4 worldPosition =
        mul(
            float4(input.position, 1.0f),
            world
        );

    output.position =
        mul(worldPosition, view);

    output.position =
        mul(output.position, proj);

    output.lightPosition =
        mul(worldPosition, lightView);

    output.lightPosition =
        mul(
            output.lightPosition,
            lightProj
        );

    output.color = input.color;

    output.worldNormal = normalize(
        mul(
            float4(input.normal, 0.0f),
            transpose(inverseWorld)
        ).xyz
    );

>>>>>>> Stashed changes
    return output;
}

float calculateShadow(
    float4 lightPosition,
    float3 normal,
    float3 directionToLight
)
{
<<<<<<< Updated upstream
    return input.color;
=======
    if (lightPosition.w <= 0.0f)
        return 1.0f;

    float3 projectedPosition =
        lightPosition.xyz /
        lightPosition.w;

    float2 shadowCoordinates;

    shadowCoordinates.x =
        projectedPosition.x * 0.5f + 0.5f;

    shadowCoordinates.y =
        -projectedPosition.y * 0.5f + 0.5f;

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

    float normalLightAmount =
        saturate(
            dot(
                normal,
                directionToLight
            )
        );

    float shadowBias =
        max(
            0.0030f *
            (1.0f - normalLightAmount),
            0.0008f
        );

    uint shadowWidth;
    uint shadowHeight;

    shadowMap.GetDimensions(
        shadowWidth,
        shadowHeight
    );

    float2 texelSize =
        1.0f /
        float2(
            shadowWidth,
            shadowHeight
        );

    float shadowAmount = 0.0f;

    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 sampleOffset =
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
    float3 normal =
        normalize(input.worldNormal);

    float3 directionToLight =
        normalize(lightDirection.xyz);

    float diffuseAmount =
        saturate(
            dot(
                normal,
                directionToLight
            )
        );

    float ambientStrength =
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

    float directLighting =
        (1.0f - ambientStrength) *
        diffuseAmount *
        shadowAmount;

    float lightingStrength =
        ambientStrength +
        directLighting;

    float3 finalColor =
        input.color.rgb *
        lightColorAndAmbient.rgb *
        lightingStrength;

    return float4(
        finalColor,
        input.color.a
    );
>>>>>>> Stashed changes
}