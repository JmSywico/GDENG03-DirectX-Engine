#include <DX3D/Game/SceneSerializer.h>

#include <DX3D/Game/World.h>
#include <DX3D/Game/GameObject.h>

#include <DX3D/Component/CameraComponent.h>
#include <DX3D/Component/CubeComponent.h>
#include <DX3D/Component/PlaneComponent.h>
#include <DX3D/Component/CombinedMeshComponent.h>
#include <DX3D/Component/DirectionalLightComponent.h>
#include <DX3D/Component/MaterialComponent.h>
#include <DX3D/Component/RigidBodyComponent.h>
#include <DX3D/Component/TransformComponent.h>

#include <DX3D/Graphics/MeshData.h>
#include <DX3D/Math/Vec3.h>
#include <DX3D/Math/Vec4.h>

#include <fstream>
#include <sstream>
#include <ostream>
#include <istream>
#include <string>
#include <vector>
#include <type_traits>
#include <utility>
#include <cstddef>
#include <unordered_map>

namespace
{
	constexpr char sceneMagic[8]
	{
		'D',
		'X',
		'3',
		'D',
		'S',
		'C',
		'N',
		'E'
	};

	constexpr dx3d::ui32 sceneVersion = 4;
	constexpr dx3d::ui32 oldestSupportedSceneVersion = 1;
	constexpr dx3d::ui32 maximumObjectCount = 100000;
	constexpr dx3d::ui32 maximumStringLength = 1024 * 1024;
	constexpr dx3d::ui32 maximumVertexCount = 10000000;
	constexpr dx3d::ui32 maximumIndexCount = 30000000;

	enum class SceneObjectType : dx3d::ui32
	{
		Cube = 1,
		Plane = 2,
		CombinedMesh = 3,
		DirectionalLight = 4
	};

	struct SerializedSceneObject
	{
		dx3d::ui64 entityId{};
		dx3d::ui64 parentEntityId{};
		SceneObjectType type{};
		std::string name{};

		dx3d::Vec3 position{};
		dx3d::Vec3 rotation{};

		dx3d::Vec3 scale
		{
			1.0f,
			1.0f,
			1.0f
		};

		dx3d::MeshData meshData{};

		dx3d::Vec3 lightColor
		{
			1.0f,
			1.0f,
			1.0f
		};

		dx3d::f32 lightIntensity{ 1.0f };
		dx3d::f32 ambientStrength{ 0.20f };
		dx3d::f32 shadowArea{ 30.0f };
		bool castShadows{ true };
		bool hasMaterial{};
		dx3d::MaterialMode materialMode{ dx3d::MaterialMode::LitTint };
		dx3d::Vec4 materialAlbedo{ 1.0f, 1.0f, 1.0f, 1.0f };
		dx3d::Vec3 materialEmissive{};
		dx3d::f32 materialEmissionStrength{};
		bool hasRigidBody{};
		dx3d::RigidBodyType rigidBodyType{ dx3d::RigidBodyType::Dynamic };
		dx3d::ColliderShape colliderShape{ dx3d::ColliderShape::Box };
		dx3d::Vec3 colliderHalfExtents{ 0.5f, 0.5f, 0.5f };
		dx3d::f32 colliderRadius{ 0.5f };
		dx3d::f32 rigidBodyMass{ 1.0f };
		dx3d::f32 rigidBodyRestitution{ 0.1f };
		bool gravityEnabled{ true };
	};

	template <typename T>
	bool writeValue(
		std::ostream& stream,
		const T& value
	)
	{
		static_assert(
			std::is_trivially_copyable_v<T>
			);

		stream.write(
			reinterpret_cast<const char*>(
				&value
				),
			sizeof(T)
		);

		return static_cast<bool>(stream);
	}

	template <typename T>
	bool readValue(
		std::istream& stream,
		T& value
	)
	{
		static_assert(
			std::is_trivially_copyable_v<T>
			);

		stream.read(
			reinterpret_cast<char*>(
				&value
				),
			sizeof(T)
		);

		return static_cast<bool>(stream);
	}

	bool writeString(
		std::ostream& stream,
		const std::string& value
	)
	{
		if (
			value.size() >
			maximumStringLength
			)
		{
			return false;
		}

		const auto length =
			static_cast<dx3d::ui32>(
				value.size()
				);

		if (!writeValue(
			stream,
			length
		))
		{
			return false;
		}

		if (length == 0)
		{
			return true;
		}

		stream.write(
			value.data(),
			length
		);

		return static_cast<bool>(stream);
	}

	bool readString(
		std::istream& stream,
		std::string& value
	)
	{
		dx3d::ui32 length = 0;

		if (!readValue(
			stream,
			length
		))
		{
			return false;
		}

		if (length > maximumStringLength)
		{
			return false;
		}

		value.resize(length);

		if (length == 0)
		{
			return true;
		}

		stream.read(
			value.data(),
			length
		);

		return static_cast<bool>(stream);
	}

	bool writeVec3(
		std::ostream& stream,
		const dx3d::Vec3& value
	)
	{
		return
			writeValue(stream, value.x) &&
			writeValue(stream, value.y) &&
			writeValue(stream, value.z);
	}

	bool readVec3(
		std::istream& stream,
		dx3d::Vec3& value
	)
	{
		return
			readValue(stream, value.x) &&
			readValue(stream, value.y) &&
			readValue(stream, value.z);
	}

	bool writeVec4(
		std::ostream& stream,
		const dx3d::Vec4& value
	)
	{
		return
			writeValue(stream, value.x) &&
			writeValue(stream, value.y) &&
			writeValue(stream, value.z) &&
			writeValue(stream, value.w);
	}

	bool readVec4(
		std::istream& stream,
		dx3d::Vec4& value
	)
	{
		return
			readValue(stream, value.x) &&
			readValue(stream, value.y) &&
			readValue(stream, value.z) &&
			readValue(stream, value.w);
	}

	bool writeMeshData(
		std::ostream& stream,
		const dx3d::MeshData& meshData
	)
	{
		if (
			meshData.vertices.size() >
			maximumVertexCount ||
			meshData.indices.size() >
			maximumIndexCount
			)
		{
			return false;
		}

		const auto vertexCount =
			static_cast<dx3d::ui32>(
				meshData.vertices.size()
				);

		if (!writeValue(
			stream,
			vertexCount
		))
		{
			return false;
		}

		for (const auto& vertex :
			meshData.vertices)
		{
			if (
				!writeVec3(
					stream,
					vertex.position
				) ||
				!writeVec4(
					stream,
					vertex.color
				) ||
				!writeVec3(
					stream,
					vertex.normal
				)
				)
			{
				return false;
			}
		}

		const auto indexCount =
			static_cast<dx3d::ui32>(
				meshData.indices.size()
				);

		if (!writeValue(
			stream,
			indexCount
		))
		{
			return false;
		}

		for (const auto index :
			meshData.indices)
		{
			if (!writeValue(
				stream,
				index
			))
			{
				return false;
			}
		}

		return true;
	}

	bool readMeshData(
		std::istream& stream,
		dx3d::MeshData& meshData
	)
	{
		dx3d::ui32 vertexCount = 0;

		if (
			!readValue(
				stream,
				vertexCount
			) ||
			vertexCount > maximumVertexCount
			)
		{
			return false;
		}

		meshData.vertices.resize(
			vertexCount
		);

		for (auto& vertex :
			meshData.vertices)
		{
			if (
				!readVec3(
					stream,
					vertex.position
				) ||
				!readVec4(
					stream,
					vertex.color
				) ||
				!readVec3(
					stream,
					vertex.normal
				)
				)
			{
				return false;
			}
		}

		dx3d::ui32 indexCount = 0;

		if (
			!readValue(
				stream,
				indexCount
			) ||
			indexCount > maximumIndexCount
			)
		{
			return false;
		}

		meshData.indices.resize(
			indexCount
		);

		for (auto& index :
			meshData.indices)
		{
			if (!readValue(
				stream,
				index
			))
			{
				return false;
			}
		}

		return true;
	}

	bool getObjectType(
		dx3d::GameObject* object,
		SceneObjectType& type
	)
	{
		if (!object)
		{
			return false;
		}

		if (
			object->getComponent<
			dx3d::CameraComponent
			>()
			)
		{
			return false;
		}

		if (
			auto* combinedMesh =
			object->getComponent<
			dx3d::CombinedMeshComponent
			>()
			)
		{
			if (!combinedMesh->hasMeshData())
			{
				return false;
			}

			type =
				SceneObjectType::CombinedMesh;

			return true;
		}

		if (
			object->getComponent<
			dx3d::CubeComponent
			>()
			)
		{
			type = SceneObjectType::Cube;
			return true;
		}

		if (
			object->getComponent<
			dx3d::PlaneComponent
			>()
			)
		{
			type = SceneObjectType::Plane;
			return true;
		}

		if (
			object->getComponent<
			dx3d::DirectionalLightComponent
			>()
			)
		{
			type =
				SceneObjectType::DirectionalLight;

			return true;
		}

		return false;
	}

	bool writeSceneObject(
		std::ostream& stream,
		dx3d::GameObject* object,
		SceneObjectType type
	)
	{
		dx3d::ui64 parentEntityId = 0;
		if (auto* parent = object->getParent())
		{
			SceneObjectType parentType{};
			if (getObjectType(parent, parentType))
				parentEntityId = parent->getEntityId();
		}

		const auto storedType =
			static_cast<dx3d::ui32>(
				type
				);

		auto* material = object->getComponent<dx3d::MaterialComponent>();
		const dx3d::ui32 hasMaterial = material ? 1u : 0u;

		if (
			!writeValue(stream, object->getEntityId()) ||
			!writeValue(stream, parentEntityId) ||
			!writeValue(stream, hasMaterial)
		)
		{
			return false;
		}

		if (material)
		{
			const auto mode = static_cast<dx3d::ui32>(material->getMode());
			if (!writeValue(stream, mode) ||
				!writeVec4(stream, material->getAlbedo()) ||
				!writeVec3(stream, material->getEmissive()) ||
				!writeValue(stream, material->getEmissionStrength()))
			{
				return false;
			}
		}

		auto* rigidBody = object->getComponent<dx3d::RigidBodyComponent>();
		const dx3d::ui32 hasRigidBody = rigidBody ? 1u : 0u;
		if (!writeValue(stream, hasRigidBody)) return false;
		if (rigidBody)
		{
			const auto bodyType = static_cast<dx3d::ui32>(rigidBody->getBodyType());
			const auto shape = static_cast<dx3d::ui32>(rigidBody->getColliderShape());
			const dx3d::ui32 gravity = rigidBody->isGravityEnabled() ? 1u : 0u;
			if (!writeValue(stream, bodyType) || !writeValue(stream, shape) ||
				!writeVec3(stream, rigidBody->getHalfExtents()) ||
				!writeValue(stream, rigidBody->getRadius()) ||
				!writeValue(stream, rigidBody->getMass()) ||
				!writeValue(stream, rigidBody->getRestitution()) ||
				!writeValue(stream, gravity))
			{
				return false;
			}
		}

		if (
			!writeValue(
				stream,
				storedType
			) ||
			!writeString(
				stream,
				object->getName()
			)
			)
		{
			return false;
		}

		auto& transform =
			object->getTransform();

		if (
			!writeVec3(
				stream,
				transform.getPosition()
			) ||
			!writeVec3(
				stream,
				transform.getRotation()
			) ||
			!writeVec3(
				stream,
				transform.getScale()
			)
			)
		{
			return false;
		}

		switch (type)
		{
		case SceneObjectType::Cube:
		case SceneObjectType::Plane:
			return true;

		case SceneObjectType::CombinedMesh:
		{
			auto* combinedMesh =
				object->getComponent<
				dx3d::CombinedMeshComponent
				>();

			return
				combinedMesh &&
				writeMeshData(
					stream,
					combinedMesh->
					getMeshData()
				);
		}

		case SceneObjectType::DirectionalLight:
		{
			auto* light =
				object->getComponent<
				dx3d::DirectionalLightComponent
				>();

			if (!light)
			{
				return false;
			}

			const dx3d::ui32 castShadows =
				light->getCastShadows()
				? 1u
				: 0u;

			return
				writeVec3(
					stream,
					light->getColor()
				) &&
				writeValue(
					stream,
					light->getIntensity()
				) &&
				writeValue(
					stream,
					light->
					getAmbientStrength()
				) &&
				writeValue(
					stream,
					light->getShadowArea()
				) &&
				writeValue(
					stream,
					castShadows
				);
		}
		}

		return false;
	}

	bool readSceneObject(
		std::istream& stream,
		SerializedSceneObject& object,
		dx3d::ui32 storedVersion
	)
	{
		if (storedVersion >= 2 &&
			(!readValue(stream, object.entityId) ||
			 !readValue(stream, object.parentEntityId)))
		{
			return false;
		}

		if (storedVersion >= 3)
		{
			dx3d::ui32 hasMaterial = 0;
			if (!readValue(stream, hasMaterial) || hasMaterial > 1u)
				return false;

			object.hasMaterial = hasMaterial != 0;
			if (object.hasMaterial)
			{
				dx3d::ui32 mode = 0;
				if (!readValue(stream, mode) ||
					mode > static_cast<dx3d::ui32>(dx3d::MaterialMode::FlatBlue) ||
					!readVec4(stream, object.materialAlbedo) ||
					!readVec3(stream, object.materialEmissive) ||
					!readValue(stream, object.materialEmissionStrength))
				{
					return false;
				}
				object.materialMode = static_cast<dx3d::MaterialMode>(mode);
			}
		}

		if (storedVersion >= 4)
		{
			dx3d::ui32 hasRigidBody = 0;
			if (!readValue(stream, hasRigidBody) || hasRigidBody > 1u) return false;
			object.hasRigidBody = hasRigidBody != 0;
			if (object.hasRigidBody)
			{
				dx3d::ui32 bodyType = 0;
				dx3d::ui32 shape = 0;
				dx3d::ui32 gravity = 0;
				if (!readValue(stream, bodyType) || bodyType > 2u ||
					!readValue(stream, shape) || shape > 1u ||
					!readVec3(stream, object.colliderHalfExtents) ||
					!readValue(stream, object.colliderRadius) ||
					!readValue(stream, object.rigidBodyMass) ||
					!readValue(stream, object.rigidBodyRestitution) ||
					!readValue(stream, gravity) || gravity > 1u)
				{
					return false;
				}
				object.rigidBodyType = static_cast<dx3d::RigidBodyType>(bodyType);
				object.colliderShape = static_cast<dx3d::ColliderShape>(shape);
				object.gravityEnabled = gravity != 0;
			}
		}

		dx3d::ui32 storedType = 0;

		if (!readValue(
			stream,
			storedType
		))
		{
			return false;
		}

		switch (
			static_cast<SceneObjectType>(
				storedType
				)
			)
		{
		case SceneObjectType::Cube:
		case SceneObjectType::Plane:
		case SceneObjectType::CombinedMesh:
		case SceneObjectType::DirectionalLight:
			object.type =
				static_cast<SceneObjectType>(
					storedType
					);
			break;

		default:
			return false;
		}

		if (
			!readString(
				stream,
				object.name
			) ||
			!readVec3(
				stream,
				object.position
			) ||
			!readVec3(
				stream,
				object.rotation
			) ||
			!readVec3(
				stream,
				object.scale
			)
			)
		{
			return false;
		}

		switch (object.type)
		{
		case SceneObjectType::Cube:
		case SceneObjectType::Plane:
			return true;

		case SceneObjectType::CombinedMesh:
			return readMeshData(
				stream,
				object.meshData
			);

		case SceneObjectType::DirectionalLight:
		{
			dx3d::ui32 castShadows = 0;

			if (
				!readVec3(
					stream,
					object.lightColor
				) ||
				!readValue(
					stream,
					object.lightIntensity
				) ||
				!readValue(
					stream,
					object.ambientStrength
				) ||
				!readValue(
					stream,
					object.shadowArea
				) ||
				!readValue(
					stream,
					castShadows
				)
				)
			{
				return false;
			}

			object.castShadows =
				castShadows != 0;

			return true;
		}
		}

		return false;
	}

	dx3d::GameObject* instantiateObject(
		dx3d::World& world,
		const SerializedSceneObject& data,
		dx3d::SceneLoadResult& result
	)
	{
		auto* object = world.createGameObjectWithId<
			dx3d::GameObject>(data.entityId);

		if (!object)
		{
			return nullptr;
		}

		object->setName(
			data.name
		);

		switch (data.type)
		{
		case SceneObjectType::Cube:
			object->createOrGetComponent<
				dx3d::CubeComponent
			>();

			++result.cubeCount;
			break;

		case SceneObjectType::Plane:
			object->createOrGetComponent<
				dx3d::PlaneComponent
			>();

			++result.planeCount;
			break;

		case SceneObjectType::CombinedMesh:
		{
			auto* combinedMesh =
				object->createOrGetComponent<
				dx3d::CombinedMeshComponent
				>();

			combinedMesh->setMeshData(
				data.meshData
			);

			break;
		}

		case SceneObjectType::DirectionalLight:
		{
			auto* light =
				object->createOrGetComponent<
				dx3d::DirectionalLightComponent
				>();

			light->setColor(
				data.lightColor
			);

			light->setIntensity(
				data.lightIntensity
			);

			light->setAmbientStrength(
				data.ambientStrength
			);

			light->setShadowArea(
				data.shadowArea
			);

			light->setCastShadows(
				data.castShadows
			);

			break;
		}
		}

		auto& transform =
			object->getTransform();

		transform.setPosition(
			data.position
		);

		transform.setRotation(
			data.rotation
		);

		transform.setScale(
			data.scale
		);

		if (data.hasMaterial)
		{
			auto* material = object->createOrGetComponent<dx3d::MaterialComponent>();
			material->setMode(data.materialMode);
			material->setAlbedo(data.materialAlbedo);
			material->setEmissive(data.materialEmissive);
			material->setEmissionStrength(data.materialEmissionStrength);
		}

		if (data.hasRigidBody)
		{
			auto* rigidBody = object->createOrGetComponent<dx3d::RigidBodyComponent>();
			rigidBody->setBodyType(data.rigidBodyType);
			rigidBody->setColliderShape(data.colliderShape);
			rigidBody->setHalfExtents(data.colliderHalfExtents);
			rigidBody->setRadius(data.colliderRadius);
			rigidBody->setMass(data.rigidBodyMass);
			rigidBody->setRestitution(data.rigidBodyRestitution);
			rigidBody->setGravityEnabled(data.gravityEnabled);
		}

		return object;
	}
}

bool dx3d::SceneSerializer::save(
	World& world,
	const std::string& filePath
)
{
	std::vector<
		std::pair<
		GameObject*,
		SceneObjectType
		>
	> objectsToSave{};

	for (auto* object :
		world.getGameObjects())
	{
		SceneObjectType type{};

		if (getObjectType(
			object,
			type
		))
		{
			objectsToSave.push_back(
				{
					object,
					type
				}
			);
		}
	}

	if (
		objectsToSave.size() >
		maximumObjectCount
		)
	{
		return false;
	}

	std::ofstream stream(
		filePath,
		std::ios::binary |
		std::ios::trunc
	);

	if (!stream)
	{
		return false;
	}

	stream.write(
		sceneMagic,
		sizeof(sceneMagic)
	);

	if (!stream)
	{
		return false;
	}

	const auto objectCount =
		static_cast<ui32>(
			objectsToSave.size()
			);

	if (
		!writeValue(
			stream,
			sceneVersion
		) ||
		!writeValue(
			stream,
			objectCount
		)
		)
	{
		return false;
	}

	for (const auto& [
		object,
		type
	] : objectsToSave)
	{
		if (!writeSceneObject(
			stream,
			object,
			type
		))
		{
			return false;
		}
	}

	return static_cast<bool>(stream);
}

std::string dx3d::SceneSerializer::serialize(
	World& world
)
{
	std::vector<std::pair<GameObject*, SceneObjectType>> objectsToSave{};

	for (auto* object : world.getGameObjects())
	{
		SceneObjectType type{};
		if (getObjectType(object, type))
		{
			objectsToSave.push_back({ object, type });
		}
	}

	if (objectsToSave.size() > maximumObjectCount)
	{
		return {};
	}

	std::ostringstream stream(
		std::ios::out | std::ios::binary
	);

	stream.write(sceneMagic, sizeof(sceneMagic));

	const auto objectCount =
		static_cast<ui32>(objectsToSave.size());

	if (!stream ||
		!writeValue(stream, sceneVersion) ||
		!writeValue(stream, objectCount))
	{
		return {};
	}

	for (const auto& [object, type] : objectsToSave)
	{
		if (!writeSceneObject(stream, object, type))
		{
			return {};
		}
	}

	return stream.str();
}

dx3d::SceneLoadResult
dx3d::SceneSerializer::load(
	World& world,
	const std::string& filePath
)
{
	SceneLoadResult result{};

	std::ifstream stream(
		filePath,
		std::ios::binary
	);

	if (!stream)
	{
		return result;
	}

	char storedMagic[
		sizeof(sceneMagic)
	]{};

		stream.read(
			storedMagic,
			sizeof(storedMagic)
		);

		if (!stream)
		{
			return result;
		}

		for (size_t index = 0;
			index < sizeof(sceneMagic);
			++index)
		{
			if (
				storedMagic[index] !=
				sceneMagic[index]
				)
			{
				return result;
			}
		}

		ui32 storedVersion = 0;
		ui32 objectCount = 0;

		if (
			!readValue(
				stream,
				storedVersion
			) ||
			!readValue(
				stream,
				objectCount
			)
			)
		{
			return result;
		}

		if (
			storedVersion < oldestSupportedSceneVersion ||
			storedVersion > sceneVersion ||
			objectCount > maximumObjectCount
			)
		{
			return result;
		}

		std::vector<
			SerializedSceneObject
		> serializedObjects{};

		serializedObjects.resize(
			objectCount
		);

		for (auto& object :
			serializedObjects)
		{
			if (!readSceneObject(
				stream,
				object,
				storedVersion
			))
			{
				return result;
			}
		}

		clear(world);
		world.update(0.0f);

		std::unordered_map<ui64, GameObject*> loadedEntities{};

		for (const auto& object :
			serializedObjects)
		{
			auto* loaded = instantiateObject(
				world,
				object,
				result
			);
			if (loaded && object.entityId != 0)
				loadedEntities[object.entityId] = loaded;
		}

		for (const auto& object : serializedObjects)
		{
			if (object.entityId == 0 || object.parentEntityId == 0)
				continue;

			const auto child = loadedEntities.find(object.entityId);
			const auto parent = loadedEntities.find(object.parentEntityId);
			if (child != loadedEntities.end() && parent != loadedEntities.end())
				world.setParent(child->second, parent->second);
		}

		result.success = true;

		return result;
}

dx3d::SceneLoadResult
dx3d::SceneSerializer::deserialize(
	World& world,
	const std::string& sceneData
)
{
	SceneLoadResult result{};

	if (sceneData.empty())
	{
		return result;
	}

	std::istringstream stream(
		sceneData,
		std::ios::in | std::ios::binary
	);

	char storedMagic[sizeof(sceneMagic)]{};
	stream.read(storedMagic, sizeof(storedMagic));

	if (!stream)
	{
		return result;
	}

	for (size_t index = 0; index < sizeof(sceneMagic); ++index)
	{
		if (storedMagic[index] != sceneMagic[index])
		{
			return result;
		}
	}

	ui32 storedVersion = 0;
	ui32 objectCount = 0;

	if (!readValue(stream, storedVersion) ||
		!readValue(stream, objectCount) ||
		storedVersion < oldestSupportedSceneVersion ||
		storedVersion > sceneVersion ||
		objectCount > maximumObjectCount)
	{
		return result;
	}

	std::vector<SerializedSceneObject> serializedObjects(objectCount);

	for (auto& object : serializedObjects)
	{
		if (!readSceneObject(stream, object, storedVersion))
		{
			return result;
		}
	}

	clear(world);
	world.update(0.0f);

	std::unordered_map<ui64, GameObject*> loadedEntities{};

	for (const auto& object : serializedObjects)
	{
		auto* loaded = instantiateObject(world, object, result);
		if (loaded && object.entityId != 0)
			loadedEntities[object.entityId] = loaded;
	}

	for (const auto& object : serializedObjects)
	{
		if (object.entityId == 0 || object.parentEntityId == 0)
			continue;

		const auto child = loadedEntities.find(object.entityId);
		const auto parent = loadedEntities.find(object.parentEntityId);
		if (child != loadedEntities.end() && parent != loadedEntities.end())
			world.setParent(child->second, parent->second);
	}

	result.success = true;
	return result;
}

void dx3d::SceneSerializer::clear(
	World& world
)
{
	for (auto* object :
		world.getGameObjects())
	{
		if (!object)
		{
			continue;
		}

		if (
			object->getComponent<
			CameraComponent
			>()
			)
		{
			continue;
		}

		world.destroyGameObject(
			object
		);
	}
}
