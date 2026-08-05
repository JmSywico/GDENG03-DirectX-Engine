#include <DX3D/Component/MaterialComponent.h>

dx3d::MaterialComponent::MaterialComponent(
	const ComponentDesc& data
)
	: Component(data)
{}

void dx3d::MaterialComponent::setTexturePath(
	const std::string& texturePath
)
{
	m_texturePath =
		texturePath;
}

const std::string&
dx3d::MaterialComponent::getTexturePath() const noexcept
{
	return m_texturePath;
}

bool dx3d::MaterialComponent::hasTexture() const noexcept
{
	return !m_texturePath.empty();
}

void dx3d::MaterialComponent::clearTexture()
{
	m_texturePath.clear();
}

void dx3d::MaterialComponent::setUvTiling(
	const Vec2& uvTiling
) noexcept
{
	m_uvTiling =
		uvTiling;
}

const dx3d::Vec2&
dx3d::MaterialComponent::getUvTiling() const noexcept
{
	return m_uvTiling;
}

void dx3d::MaterialComponent::setUvOffset(
	const Vec2& uvOffset
) noexcept
{
	m_uvOffset =
		uvOffset;
}

const dx3d::Vec2&
dx3d::MaterialComponent::getUvOffset() const noexcept
{
	return m_uvOffset;
}
