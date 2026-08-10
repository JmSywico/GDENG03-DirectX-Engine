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
    float3 worldPosition : TEXCOORD2;
};

cbuffer ConstantData : register(b0)
{
    row_major float4x4 world;
    row_major float4x4 view;
    row_major float4x4 proj;

    float4 lightDirections[16];
    float4 lightColors[16];
    float4 lightPositions[16];
    float4 lightParameters[16];
    float4 lightMeta;
    float4 materialAlbedo;
    float4 materialEmissiveAndStrength;
    float4 materialParameters;

    row_major float4x4 inverseWorld;

    row_major float4x4 lightView;
    row_major float4x4 lightProj;
};

Texture2D shadowMap : register(t0);
Texture2D albedoMap : register(t1);
TextureCube pointShadowMap : register(t2);

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
    output.worldPosition = worldPosition.xyz;

    return output;
}

float calculateProjectedShadow(
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

float calculatePointShadow(
    float3 worldPosition,
    float3 normal,
    float3 directionToLight,
    float3 lightPosition,
    float lightRange,
    float nearPlane
)
{
    const float3 lightToFragment = worldPosition - lightPosition;
    const float3 absoluteDirection = abs(lightToFragment);
    const float faceDepth = max(
        absoluteDirection.x,
        max(absoluteDirection.y, absoluteDirection.z));
    if (faceDepth <= nearPlane || faceDepth >= lightRange)
        return 1.0f;

    const float projectedDepth =
        lightRange / (lightRange - nearPlane) -
        (nearPlane * lightRange) /
        ((lightRange - nearPlane) * faceDepth);
    const float normalLightAmount = saturate(dot(normal, directionToLight));
    const float shadowBias = max(
        0.0030f * (1.0f - normalLightAmount), 0.0008f);

    const float3 sampleDirection = normalize(lightToFragment);
    const float3 referenceAxis = abs(sampleDirection.y) < 0.99f
        ? float3(0.0f, 1.0f, 0.0f)
        : float3(1.0f, 0.0f, 0.0f);
    const float3 tangent = normalize(cross(referenceAxis, sampleDirection));
    const float3 bitangent = cross(sampleDirection, tangent);
    const float texelSize = 1.5f / 1024.0f;

    float shadowAmount = 0.0f;
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            const float3 offsetDirection = normalize(
                sampleDirection +
                (tangent * x + bitangent * y) * texelSize);
            shadowAmount += pointShadowMap.SampleCmpLevelZero(
                shadowSampler,
                offsetDirection,
                projectedDepth - shadowBias);
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

    const float ambientStrength =
        saturate(
            lightMeta.y
        );

    float3 directLighting = 0.0f;
    const int lightCount = min((int)(lightMeta.x + 0.5f), 16);
    const int shadowLightIndex = (int)lightMeta.z;
    [loop]
    for (int lightIndex = 0; lightIndex < lightCount; ++lightIndex)
    {
        const float4 directionIntensity = lightDirections[lightIndex];
        const float4 positionRange = lightPositions[lightIndex];
        const float4 parameters = lightParameters[lightIndex];
        const int lightType = (int)(parameters.x + 0.5f);
        float3 directionToLight = normalize(-directionIntensity.xyz);
        float attenuation = 1.0f;

        if (lightType != 0)
        {
            const float3 toLight = positionRange.xyz - input.worldPosition;
            const float distanceToLight = length(toLight);
            directionToLight = distanceToLight > 0.0001f
                ? toLight / distanceToLight
                : float3(0.0f, 0.0f, 1.0f);
            const float normalizedDistance = distanceToLight / max(positionRange.w, 0.0001f);
            const float rangeFalloff = saturate(1.0f - normalizedDistance * normalizedDistance);
            attenuation = rangeFalloff * rangeFalloff;

            if (lightType == 2)
            {
                const float coneCosine = dot(normalize(directionIntensity.xyz), -directionToLight);
                const float outerCosine = parameters.y;
                const float innerCosine = lerp(outerCosine, 1.0f, 0.2f);
                attenuation *= smoothstep(outerCosine, innerCosine, coneCosine);
            }
        }

        const float diffuseAmount = saturate(dot(normal, directionToLight));
        float shadowAmount = 1.0f;
        if (lightMeta.w > 0.5f && lightIndex == shadowLightIndex)
        {
            shadowAmount = lightType == 1
                ? calculatePointShadow(
                    input.worldPosition,
                    normal,
                    directionToLight,
                    positionRange.xyz,
                    positionRange.w,
                    parameters.z)
                : calculateProjectedShadow(
                    input.lightPosition,
                    normal,
                    directionToLight);
        }
        directLighting += lightColors[lightIndex].rgb * directionIntensity.w
            * diffuseAmount * attenuation * shadowAmount;
    }

    float3 surfaceColor = input.color.rgb * materialAlbedo.rgb;
    if (materialParameters.y > 0.5f)
    {
        const float3 blend = abs(normal);
        const float2 uv = blend.y >= blend.x && blend.y >= blend.z
            ? input.objectPosition.xz + 0.5f
            : (blend.x >= blend.z ? input.objectPosition.zy + 0.5f : input.objectPosition.xy + 0.5f);
        surfaceColor *= albedoMap.Sample(albedoSampler, uv).rgb;
    }
    float3 appliedLighting = ambientStrength.xxx + directLighting;

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
        surfaceColor * appliedLighting +
        materialEmissiveAndStrength.rgb * materialEmissiveAndStrength.a;

    return float4(
        finalColor,
        input.color.a * materialAlbedo.a
    );
}
