#include <DX3D/Component/MaterialComponent.h>

#include <algorithm>

dx3d::MaterialComponent::MaterialComponent(const ComponentDesc& data)
	: Component(data)
{}

void dx3d::MaterialComponent::setMode(MaterialMode mode) noexcept
{
	m_mode = mode;
}

dx3d::MaterialMode dx3d::MaterialComponent::getMode() const noexcept
{
	return m_mode;
}

void dx3d::MaterialComponent::setAlbedo(const Vec4& albedo) noexcept
{
	m_albedo =
	{
		std::clamp(albedo.x, 0.0f, 1.0f),
		std::clamp(albedo.y, 0.0f, 1.0f),
		std::clamp(albedo.z, 0.0f, 1.0f),
		std::clamp(albedo.w, 0.0f, 1.0f)
	};
}

dx3d::Vec4 dx3d::MaterialComponent::getAlbedo() const noexcept
{
	return m_albedo;
}

void dx3d::MaterialComponent::setEmissive(const Vec3& emissive) noexcept
{
	m_emissive =
	{
		std::max(0.0f, emissive.x),
		std::max(0.0f, emissive.y),
		std::max(0.0f, emissive.z)
	};
}

dx3d::Vec3 dx3d::MaterialComponent::getEmissive() const noexcept
{
	return m_emissive;
}

void dx3d::MaterialComponent::setEmissionStrength(f32 strength) noexcept
{
	m_emissionStrength = std::clamp(strength, 0.0f, 100.0f);
}

dx3d::f32 dx3d::MaterialComponent::getEmissionStrength() const noexcept
{
	return m_emissionStrength;
}
