#include <DX3D/Component/TransformComponent.h>
#include <DX3D/Game/World.h>
#include <DX3D/Game/GameObject.h>

#include <cmath>


dx3d::TransformComponent::TransformComponent(const ComponentDesc& data) : Component(data)
{
	markAsDirty();
}

void dx3d::TransformComponent::setPosition(const Vec3& position)
{
	if (!std::isfinite(position.x) || !std::isfinite(position.y)
		|| !std::isfinite(position.z)) return;
	m_position = position;
	markAsDirty();
}

dx3d::Vec3 dx3d::TransformComponent::getPosition() const noexcept
{
	return m_position;
}

void dx3d::TransformComponent::setRotation(const Vec3& rotation)
{
	if (!std::isfinite(rotation.x) || !std::isfinite(rotation.y)
		|| !std::isfinite(rotation.z)) return;
	m_rotation = rotation;
	const f32 halfPitch = rotation.x * 0.5f;
	const f32 halfYaw = rotation.y * 0.5f;
	const f32 halfRoll = rotation.z * 0.5f;
	const f32 cp = std::cos(halfPitch), sp = std::sin(halfPitch);
	const f32 cy = std::cos(halfYaw), sy = std::sin(halfYaw);
	const f32 cr = std::cos(halfRoll), sr = std::sin(halfRoll);
	// Match the row-vector rotation convention used by Mat4x4. In particular,
	// combined pitch and yaw must keep the camera's right axis horizontal.
	m_rotationQuaternion = {
		sp * cy * cr + cp * sy * sr,
		cp * sy * cr - sp * cy * sr,
		cp * cy * sr - sp * sy * cr,
		cp * cy * cr + sp * sy * sr
	};
	markAsDirty();
}

dx3d::Vec3 dx3d::TransformComponent::getRotation() const noexcept
{
	return m_rotation;
}

void dx3d::TransformComponent::setRotationQuaternion(const Vec4& rotation, const Vec3& eulerHint)
{
	if (!std::isfinite(rotation.x) || !std::isfinite(rotation.y)
		|| !std::isfinite(rotation.z) || !std::isfinite(rotation.w)
		|| !std::isfinite(eulerHint.x) || !std::isfinite(eulerHint.y)
		|| !std::isfinite(eulerHint.z)) return;
	const f32 lengthSquared = rotation.x * rotation.x + rotation.y * rotation.y
		+ rotation.z * rotation.z + rotation.w * rotation.w;
	if (!std::isfinite(lengthSquared) || lengthSquared <= 0.000001f) return;
	const f32 inverseLength = 1.0f / std::sqrt(lengthSquared);
	m_rotationQuaternion = {
		rotation.x * inverseLength, rotation.y * inverseLength,
		rotation.z * inverseLength, rotation.w * inverseLength
	};
	m_rotation = eulerHint;
	markAsDirty();
}

dx3d::Vec4 dx3d::TransformComponent::getRotationQuaternion() const noexcept
{
	return m_rotationQuaternion;
}

void dx3d::TransformComponent::setScale(const Vec3& scale)
{
	if (!std::isfinite(scale.x) || !std::isfinite(scale.y)
		|| !std::isfinite(scale.z)) return;
	m_scale = scale;
	markAsDirty();
}

dx3d::Vec3 dx3d::TransformComponent::getScale() const noexcept
{
	return m_scale;
}


dx3d::Vec3 dx3d::TransformComponent::forward()
{
	auto forward = getRigidWorldMatrix().row(2);
	return dx3d::Vec3::normalize({ forward.x,forward.y,forward.z});
}

dx3d::Vec3 dx3d::TransformComponent::right()
{
	auto right = getRigidWorldMatrix().row(0);
	return dx3d::Vec3::normalize({ right.x,right.y,right.z });
}

dx3d::Vec3 dx3d::TransformComponent::up()
{
	auto up = getRigidWorldMatrix().row(1);
	return dx3d::Vec3::normalize({ up.x,up.y,up.z });
}

dx3d::Mat4x4 dx3d::TransformComponent::getAffineWorldMatrix() noexcept
{
	updateWorldMatrix();
	return m_affineWorldMatrix;
}

dx3d::Mat4x4 dx3d::TransformComponent::getRigidWorldMatrix() noexcept
{
	updateWorldMatrix();
	return m_rigidWorldMatrix;;
}



void dx3d::TransformComponent::updateWorldMatrix() noexcept
{
	if (!m_dirty) return;

	m_dirty = false;

	const Mat4x4 localRigid =
		Mat4x4::rotateQuaternion(m_rotationQuaternion) *
		Mat4x4::translate(m_position);

	const Mat4x4 localAffine =
		Mat4x4::scale(m_scale) *
		localRigid;

	if (auto* parent = m_object.getParent())
	{
		m_rigidWorldMatrix =
			localRigid * parent->getTransform().getRigidWorldMatrix();
		m_affineWorldMatrix =
			localAffine * parent->getTransform().getAffineWorldMatrix();
	}
	else
	{
		m_rigidWorldMatrix = localRigid;
		m_affineWorldMatrix = localAffine;
	}
}


void dx3d::TransformComponent::markAsDirty()
{
	m_world.addDirtyTransformInternal(*this);
}
