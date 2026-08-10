$input v_normal, v_texcoord0, v_tangent, v_bitangent, v_worldPosition, v_viewDepth

#include "bgfx_shader.sh"

uniform vec4 u_albedo;
uniform vec4 u_lightDirIntensity;
uniform vec4 u_lightColorMaterial;
uniform vec4 u_lightPositionRange;
uniform vec4 u_lightTypeSpot;
uniform vec4 u_materialSurface;
uniform vec4 u_materialEmissive;
uniform vec4 u_cameraPosition;
uniform vec4 u_debugView;
uniform mat4 u_shadowMtx0;
uniform mat4 u_shadowMtx1;
uniform mat4 u_shadowMtx2;
uniform mat4 u_shadowMtx3;
uniform vec4 u_shadowParams;
uniform vec4 u_cascadeSplits;
uniform vec4 u_shadowMapInfo;
SAMPLER2D(s_albedo, 0);
SAMPLER2D(s_metallicRoughness, 1);
SAMPLER2D(s_normal, 2);
SAMPLER2DSHADOW(s_shadowMap0, 3);
SAMPLER2DSHADOW(s_shadowMap1, 4);
SAMPLER2DSHADOW(s_shadowMap2, 5);
SAMPLER2DSHADOW(s_shadowMap3, 6);

#define ShadowSampler sampler2DShadow

float sampleShadowPcf(
	ShadowSampler _sampler,
	mat4 shadowMatrix,
	vec3 worldPosition,
	vec3 geometricNormal,
	float depthBias)
{
	vec3 offsetPosition = worldPosition + geometricNormal * u_shadowParams.w;
	vec4 projected = mul(shadowMatrix, vec4(offsetPosition, 1.0));
	vec3 coordinate = projected.xyz / projected.w;
	if (projected.w <= 0.0
		|| coordinate.x <= 0.0 || coordinate.x >= 1.0
		|| coordinate.y <= 0.0 || coordinate.y >= 1.0
		|| coordinate.z <= 0.0 || coordinate.z >= 1.0)
		return 1.0;

	float visibility = 0.0;
	float texel = u_shadowMapInfo.x;
	for (int y = -1; y <= 1; ++y)
	{
		for (int x = -1; x <= 1; ++x)
		{
			vec2 offset = vec2(float(x), float(y)) * texel;
			visibility += shadow2D(_sampler, vec3(coordinate.xy + offset, coordinate.z - depthBias));
		}
	}
	return visibility * (1.0 / 9.0);
}

float sampleCascade(int cascade, vec3 worldPosition, vec3 geometricNormal, float bias)
{
	if (cascade == 0) return sampleShadowPcf(s_shadowMap0, u_shadowMtx0, worldPosition, geometricNormal, bias);
	if (cascade == 1) return sampleShadowPcf(s_shadowMap1, u_shadowMtx1, worldPosition, geometricNormal, bias);
	if (cascade == 2) return sampleShadowPcf(s_shadowMap2, u_shadowMtx2, worldPosition, geometricNormal, bias);
	return sampleShadowPcf(s_shadowMap3, u_shadowMtx3, worldPosition, geometricNormal, bias);
}

float shadowVisibility(vec3 worldPosition, vec3 geometricNormal, float nDotL, float viewDepth)
{
	if (u_shadowParams.x < 0.5)
		return 1.0;
	if (nDotL <= 0.0)
		return 1.0;
	float bias = u_shadowParams.z * (1.0 + (1.0 - nDotL) * 2.0);
	if (u_shadowMapInfo.y < 0.5)
		return mix(1.0, sampleCascade(0, worldPosition, geometricNormal, bias), u_shadowParams.y);

	int cascadeCount = int(u_shadowMapInfo.z + 0.5);
	float lastSplit = cascadeCount <= 1 ? u_cascadeSplits.x
		: (cascadeCount == 2 ? u_cascadeSplits.y
		: (cascadeCount == 3 ? u_cascadeSplits.z : u_cascadeSplits.w));
	if (viewDepth > lastSplit)
		return 1.0;
	int cascade = 0;
	if (cascadeCount > 1 && viewDepth > u_cascadeSplits.x) cascade = 1;
	if (cascadeCount > 2 && viewDepth > u_cascadeSplits.y) cascade = 2;
	if (cascadeCount > 3 && viewDepth > u_cascadeSplits.z) cascade = 3;

	float visibility = sampleCascade(cascade, worldPosition, geometricNormal, bias);
	if (cascade + 1 < cascadeCount)
	{
		float splitNear = cascade == 0 ? 0.0
			: (cascade == 1 ? u_cascadeSplits.x
			: (cascade == 2 ? u_cascadeSplits.y : u_cascadeSplits.z));
		float splitFar = cascade == 0 ? u_cascadeSplits.x
			: (cascade == 1 ? u_cascadeSplits.y
			: (cascade == 2 ? u_cascadeSplits.z : u_cascadeSplits.w));
		float blendWidth = max((splitFar - splitNear) * u_shadowMapInfo.w, 0.0001);
		float blend = clamp((viewDepth - (splitFar - blendWidth)) / blendWidth, 0.0, 1.0);
		if (blend > 0.0)
			visibility = mix(visibility, sampleCascade(cascade + 1, worldPosition, geometricNormal, bias), blend);
	}
	return mix(1.0, visibility, u_shadowParams.y);
}

vec3 rainbow(vec2 uv)
{
	vec3 phase = vec3(0.0, 0.33, 0.67);
	return 0.5 + 0.5 * cos(6.28318 * (uv.x + phase));
}

float distributionGGX(vec3 normal, vec3 halfway, float roughness)
{
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float nDotH = max(dot(normal, halfway), 0.0);
	float denominator = nDotH * nDotH * (alpha2 - 1.0) + 1.0;
	return alpha2 / max(3.14159265 * denominator * denominator, 0.000001);
}

float geometrySchlickGGX(float nDotDirection, float roughness)
{
	float radius = roughness + 1.0;
	float k = radius * radius * 0.125;
	return nDotDirection / max(nDotDirection * (1.0 - k) + k, 0.000001);
}

vec3 fresnelSchlick(float cosine, vec3 f0)
{
	return f0 + (vec3(1.0, 1.0, 1.0) - f0)
		* pow(clamp(1.0 - cosine, 0.0, 1.0), 5.0);
}

vec3 srgbToLinear(vec3 value)
{
	vec3 positive = max(value, vec3(0.0, 0.0, 0.0));
	vec3 low = positive * (1.0 / 12.92);
	vec3 high = pow((positive + vec3(0.055, 0.055, 0.055)) * (1.0 / 1.055), vec3(2.4, 2.4, 2.4));
	return mix(low, high, step(vec3(0.04045, 0.04045, 0.04045), positive));
}

vec3 linearToSrgb(vec3 value)
{
	vec3 positive = max(value, vec3(0.0, 0.0, 0.0));
	vec3 low = positive * 12.92;
	vec3 high = vec3(1.055, 1.055, 1.055) * pow(positive, vec3(1.0 / 2.4, 1.0 / 2.4, 1.0 / 2.4))
		- vec3(0.055, 0.055, 0.055);
	return mix(low, high, step(vec3(0.0031308, 0.0031308, 0.0031308), positive));
}

void main()
{
	if (u_debugView.x > 0.5)
	{
		gl_FragColor = vec4(normalize(v_normal) * 0.5 + 0.5, 1.0);
		return;
	}

	vec3 shadingNormal = normalize(v_normal);
	if (u_materialSurface.w > 0.5)
	{
		vec3 tangentNormal = texture2D(s_normal, v_texcoord0).xyz * 2.0 - 1.0;
		shadingNormal = normalize(
			v_tangent * tangentNormal.x
			+ v_bitangent * tangentNormal.y
			+ v_normal * tangentNormal.z);
	}

	float materialMode = u_lightColorMaterial.w;
	if (materialMode > 0.5 && materialMode < 1.5)
	{
		gl_FragColor = vec4(rainbow(v_texcoord0), 1.0);
		return;
	}
	if (materialMode > 1.5 && materialMode < 2.5)
	{
		gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);
		return;
	}
	if (materialMode > 2.5 && materialMode < 3.5)
	{
		gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
		return;
	}
	if (materialMode > 3.5)
	{
		gl_FragColor = vec4(0.0, 0.0, 1.0, 1.0);
		return;
	}

	float lightType = u_lightTypeSpot.x;
	vec3 lightDirection = normalize(-u_lightDirIntensity.xyz);
	float attenuation = 1.0;
	if (lightType > 0.5)
	{
		vec3 toLight = u_lightPositionRange.xyz - v_worldPosition;
		float distanceToLight = length(toLight);
		lightDirection = distanceToLight > 0.0001
			? toLight / distanceToLight
			: vec3(0.0, 0.0, 1.0);
		float normalizedDistance = distanceToLight / max(u_lightPositionRange.w, 0.0001);
		float rangeFalloff = max(1.0 - normalizedDistance * normalizedDistance, 0.0);
		attenuation = rangeFalloff * rangeFalloff;
		if (lightType > 1.5)
		{
			vec3 lightToSurface = -lightDirection;
			float coneCosine = dot(normalize(u_lightDirIntensity.xyz), lightToSurface);
			float outerCosine = u_lightTypeSpot.y;
			float innerCosine = mix(outerCosine, 1.0, 0.2);
			attenuation *= smoothstep(outerCosine, innerCosine, coneCosine);
		}
	}
	float nDotL = max(dot(shadingNormal, lightDirection), 0.0);
	float visibility = shadowVisibility(v_worldPosition, normalize(v_normal), nDotL, v_viewDepth);
	vec4 sampledTexture = texture2D(s_albedo, v_texcoord0);
	if (u_materialEmissive.w > 0.5)
		sampledTexture.rgb = srgbToLinear(sampledTexture.rgb);
	vec3 albedo = sampledTexture.rgb * srgbToLinear(u_albedo.rgb);
	float alpha = sampledTexture.a * u_albedo.a;
	vec2 metallicRoughness = u_materialSurface.xy;
	if (u_materialSurface.z > 0.5)
	{
		vec4 sampledSurface = texture2D(s_metallicRoughness, v_texcoord0);
		metallicRoughness.x *= sampledSurface.b;
		metallicRoughness.y *= sampledSurface.g;
	}
	float metallic = clamp(metallicRoughness.x, 0.0, 1.0);
	float roughness = clamp(metallicRoughness.y, 0.04, 1.0);
	vec3 viewDirection = normalize(u_cameraPosition.xyz - v_worldPosition);
	vec3 halfway = normalize(viewDirection + lightDirection);
	float nDotV = max(dot(shadingNormal, viewDirection), 0.0001);
	float hDotV = max(dot(halfway, viewDirection), 0.0);
	vec3 f0 = mix(vec3(0.04, 0.04, 0.04), albedo, metallic);
	vec3 fresnel = fresnelSchlick(hDotV, f0);
	float distribution = distributionGGX(shadingNormal, halfway, roughness);
	float geometry = geometrySchlickGGX(nDotV, roughness)
		* geometrySchlickGGX(nDotL, roughness);
	vec3 specular = distribution * geometry * fresnel / max(4.0 * nDotV * nDotL, 0.0001);
	vec3 diffuse = (vec3(1.0, 1.0, 1.0) - fresnel) * (1.0 - metallic)
		* albedo * (1.0 / 3.14159265);
	vec3 radiance = srgbToLinear(u_lightColorMaterial.rgb) * u_lightDirIntensity.w * attenuation;
	vec3 ambient = albedo * (1.0 - metallic) * 0.03;
	vec3 color = ambient + (diffuse + specular) * radiance * nDotL * visibility
		+ srgbToLinear(u_materialEmissive.rgb);
	color = linearToSrgb(color);
	gl_FragColor = vec4(color, alpha);
}
