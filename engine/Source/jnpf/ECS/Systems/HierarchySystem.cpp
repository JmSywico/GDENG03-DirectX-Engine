#include "ECS/Systems/HierarchySystem.h"
#include <algorithm>

namespace jnpf::ECS
{
	void HierarchySystem::Update(entt::registry& registry, float deltaTime)
	{
		UNREFERENCED_PARAMETER(registry);
		UNREFERENCED_PARAMETER(deltaTime);
	}

	void HierarchySystem::ReparentKeepWorldTransform(
		entt::registry& registry,
		entt::entity child,
		entt::entity newParent)
	{
		if (!registry.valid(child) || (newParent != entt::null && !registry.valid(newParent)))
			return;
		if (newParent == child || (newParent != entt::null && IsAncestor(registry, child, newParent)))
			return;

		auto* childTransform = registry.try_get<Scene::TransformComponent>(child);
		if (!childTransform)
			return;

		DirectX::XMMATRIX childWorldMatrix = childTransform->GetWorldMatrix();

		DirectX::XMMATRIX parentWorldMatrix = DirectX::XMMatrixIdentity();
		if (newParent != entt::null)
		{
			auto* parentTransform = registry.try_get<Scene::TransformComponent>(newParent);
			if (parentTransform)
				parentWorldMatrix = parentTransform->GetWorldMatrix();
		}

		DirectX::XMMATRIX parentWorldInverse = DirectX::XMMatrixInverse(nullptr, parentWorldMatrix);
		DirectX::XMMATRIX newLocalMatrix = DirectX::XMMatrixMultiply(childWorldMatrix, parentWorldInverse);

		DirectX::XMVECTOR scale, rotation, position;
		if (!DirectX::XMMatrixDecompose(&scale, &rotation, &position, newLocalMatrix))
			return;

		DirectX::XMFLOAT3 eulerRotation;
		DirectX::XMFLOAT4 quat;
		DirectX::XMStoreFloat4(&quat, DirectX::XMQuaternionNormalize(rotation));

		const float pitch = atan2f(2.0f * (quat.w * quat.x + quat.y * quat.z),
			1.0f - 2.0f * (quat.x * quat.x + quat.y * quat.y));
		const float yaw = asinf(std::clamp(
			2.0f * (quat.w * quat.y - quat.z * quat.x), -1.0f, 1.0f));
		const float roll = atan2f(2.0f * (quat.w * quat.z + quat.x * quat.y),
			1.0f - 2.0f * (quat.y * quat.y + quat.z * quat.z));

		eulerRotation = {pitch, yaw, roll};

		DirectX::XMFLOAT3 newPosition;
		DirectX::XMStoreFloat3(&newPosition, position);
		childTransform->SetLocalPosition(newPosition);
		childTransform->SetLocalRotationQuaternion(quat, eulerRotation);

		DirectX::XMFLOAT3 newScale;
		DirectX::XMStoreFloat3(&newScale, scale);
		childTransform->SetLocalScale(newScale);

		DetachChild(registry, child);
		AttachChild(registry, newParent, child);
		MarkSubtreeDirty(registry, child);
	}

	void HierarchySystem::ReparentKeepLocalTransform(
		entt::registry& registry,
		entt::entity child,
		entt::entity newParent)
	{
		if (!registry.valid(child) || (newParent != entt::null && !registry.valid(newParent)))
			return;
		if (newParent == child || (newParent != entt::null && IsAncestor(registry, child, newParent)))
			return;

		DetachChild(registry, child);
		AttachChild(registry, newParent, child);
		MarkSubtreeDirty(registry, child);
	}

	void HierarchySystem::RemoveChild(entt::registry& registry, entt::entity parent, entt::entity child)
	{
		if (!registry.valid(parent) || !registry.valid(child))
			return;
		auto* parentHier = registry.try_get<Scene::HierarchyComponent>(parent);
		if (!parentHier)
			return;

		auto* childHier = registry.try_get<Scene::HierarchyComponent>(child);
		if (!childHier || childHier->Parent != parent)
			return;

		DetachChild(registry, child);
	}

	entt::entity HierarchySystem::GetParent(const entt::registry& registry, entt::entity entity)
	{
		if (!registry.valid(entity))
			return entt::null;
		const auto* hier = registry.try_get<Scene::HierarchyComponent>(entity);
		if (hier && hier->Parent != entt::null && registry.valid(hier->Parent))
			return hier->Parent;
		return entt::null;
	}

	std::vector<entt::entity> HierarchySystem::GetChildren(const entt::registry& registry, entt::entity entity)
	{
		std::vector<entt::entity> children;

		if (!registry.valid(entity))
			return children;
		const auto* hier = registry.try_get<Scene::HierarchyComponent>(entity);
		if (!hier || hier->FirstChild == entt::null)
			return children;

		entt::entity current = hier->FirstChild;
		while (current != entt::null && registry.valid(current))
		{
			children.push_back(current);

			const auto* currentHier = registry.try_get<Scene::HierarchyComponent>(current);
			if (!currentHier)
				break;

			current = currentHier->NextSibling;
		}

		return children;
	}

	void HierarchySystem::TraverseDescendants(
		const entt::registry& registry,
		entt::entity entity,
		const std::function<void(entt::entity)>& callback)
	{
		auto children = GetChildren(registry, entity);
		for (const auto& child : children)
		{
			callback(child);
			TraverseDescendants(registry, child, callback);
		}
	}

	bool HierarchySystem::IsAncestor(
		const entt::registry& registry,
		entt::entity ancestor,
		entt::entity entity)
	{
		if (ancestor == entt::null || entity == entt::null)
			return false;
		entt::entity current = entity;
		while (current != entt::null && registry.valid(current))
		{
			const auto* hier = registry.try_get<Scene::HierarchyComponent>(current);
			if (!hier)
				break;

			if (hier->Parent == ancestor)
				return true;

			current = hier->Parent;
		}
		return false;
	}

	void HierarchySystem::DetachChild(entt::registry& registry, entt::entity child)
	{
		if (!registry.valid(child))
			return;
		auto* childHier = registry.try_get<Scene::HierarchyComponent>(child);
		if (!childHier || childHier->Parent == entt::null)
			return;

		const entt::entity parent = childHier->Parent;
		auto* parentHier = registry.valid(parent) ? registry.try_get<Scene::HierarchyComponent>(parent) : nullptr;
		if (!parentHier)
		{
			childHier->Parent = entt::null;
			childHier->NextSibling = entt::null;
			childHier->PrevSibling = entt::null;
			return;
		}

		if (parentHier->FirstChild == child)
		{
			parentHier->FirstChild = childHier->NextSibling;
		}
		else
		{
			if (childHier->PrevSibling != entt::null && registry.valid(childHier->PrevSibling))
			{
				auto* prevHier = registry.try_get<Scene::HierarchyComponent>(childHier->PrevSibling);
				if (prevHier)
				{
					prevHier->NextSibling = childHier->NextSibling;
				}
			}
		}

		if (childHier->NextSibling != entt::null && registry.valid(childHier->NextSibling))
		{
			auto* nextHier = registry.try_get<Scene::HierarchyComponent>(childHier->NextSibling);
			if (nextHier)
			{
				nextHier->PrevSibling = childHier->PrevSibling;
			}
		}

		parentHier->ChildCount = (parentHier->ChildCount > 0) ? parentHier->ChildCount - 1 : 0;

		childHier->Parent = entt::null;
		childHier->NextSibling = entt::null;
		childHier->PrevSibling = entt::null;
	}

	void HierarchySystem::AttachChild(entt::registry& registry, entt::entity parent, entt::entity child)
	{
		auto* childHier = registry.valid(child) ? registry.try_get<Scene::HierarchyComponent>(child) : nullptr;
		if (!childHier)
			return;

		if (parent == entt::null)
		{
			childHier->Parent = entt::null;
			childHier->NextSibling = entt::null;
			childHier->PrevSibling = entt::null;
			return;
		}

		if (!registry.valid(parent) || parent == child || IsAncestor(registry, child, parent))
			return;

		auto* parentHier = registry.try_get<Scene::HierarchyComponent>(parent);
		if (!parentHier)
			return;

		childHier->Parent = parent;
		childHier->NextSibling = parentHier->FirstChild;
		childHier->PrevSibling = entt::null;

		if (parentHier->FirstChild != entt::null && registry.valid(parentHier->FirstChild))
		{
			auto* oldFirstHier = registry.try_get<Scene::HierarchyComponent>(parentHier->FirstChild);
			if (oldFirstHier)
			{
				oldFirstHier->PrevSibling = child;
			}
		}

		parentHier->FirstChild = child;
		parentHier->ChildCount++;
	}

	void HierarchySystem::MarkSubtreeDirty(entt::registry& registry, entt::entity entity)
	{
		auto* transform = registry.valid(entity) ? registry.try_get<Scene::TransformComponent>(entity) : nullptr;
		if (transform)
		{
			transform->MarkWorldTransformDirty();
		}

		TraverseDescendants(registry, entity, [&registry](entt::entity descendant)
		{
			auto* descTransform = registry.try_get<Scene::TransformComponent>(descendant);
			if (descTransform)
			{
				descTransform->MarkWorldTransformDirty();
			}
		});
	}
}
