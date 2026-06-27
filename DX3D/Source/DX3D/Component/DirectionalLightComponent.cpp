#include <DX3D/Component/DirectionalLightComponent.h>

dx3d::DirectionalLightComponent::
DirectionalLightComponent(
	const ComponentDesc& data
)
	: Component(data)
{}

void dx3d::DirectionalLightComponent::setColor(
	const Vec3& color
) noexcept
{
	m_color =
	{
		color.x < 0.0f ? 0.0f : color.x,
		color.y < 0.0f ? 0.0f : color.y,
		color.z < 0.0f ? 0.0f : color.z
	};
}

const dx3d::Vec3&
dx3d::DirectionalLightComponent::getColor() const noexcept
{
	return m_color;
}

void dx3d::DirectionalLightComponent::setIntensity(
	f32 intensity
) noexcept
{
	if (intensity < 0.0f)
		intensity = 0.0f;

	m_intensity = intensity;
}

dx3d::f32
dx3d::DirectionalLightComponent::getIntensity() const noexcept
{
	return m_intensity;
}

void dx3d::DirectionalLightComponent::setAmbientStrength(
	f32 ambientStrength
) noexcept
{
	if (ambientStrength < 0.0f)
		ambientStrength = 0.0f;

	if (ambientStrength > 1.0f)
		ambientStrength = 1.0f;

	m_ambientStrength = ambientStrength;
}

dx3d::f32
dx3d::DirectionalLightComponent::
getAmbientStrength() const noexcept
{
	return m_ambientStrength;
}

void dx3d::DirectionalLightComponent::setShadowArea(
	f32 shadowArea
) noexcept
{
	if (shadowArea <= 0.0f)
		return;

	m_shadowArea = shadowArea;
}

dx3d::f32
dx3d::DirectionalLightComponent::getShadowArea() const noexcept
{
	return m_shadowArea;
}

void dx3d::DirectionalLightComponent::setCastShadows(
	bool castShadows
) noexcept
{
	m_castShadows = castShadows;
}

bool dx3d::DirectionalLightComponent::
getCastShadows() const noexcept
{
	return m_castShadows;
}