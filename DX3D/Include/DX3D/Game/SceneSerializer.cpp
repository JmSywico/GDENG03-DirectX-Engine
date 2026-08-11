#include <DX3D/Game/SceneSerializer.h>

#include <DX3D/Game/World.h>
#include <DX3D/Game/GameObject.h>

#include <DX3D/Component/CameraComponent.h>
#include <DX3D/Component/CubeComponent.h>
#include <DX3D/Component/SphereComponent.h>
#include <DX3D/Component/CylinderComponent.h>
#include <DX3D/Component/CapsuleComponent.h>
#include <DX3D/Component/PlaneComponent.h>
#include <DX3D/Component/CombinedMeshComponent.h>
#include <DX3D/Component/DirectionalLightComponent.h>
#include <DX3D/Component/MaterialComponent.h>
#include <DX3D/Component/RigidBodyComponent.h>
#include <DX3D/Component/ColliderComponent.h>
#include <DX3D/Component/RotatorComponent.h>
#include <DX3D/Component/FlyControllerComponent.h>
#include <DX3D/Component/TextureComponent.h>
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
#include <unordered_set>

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

	constexpr dx3d::ui32 sceneVersion = 12;
	constexpr dx3d::ui32 oldestSupportedSceneVersion = 1;
	constexpr dx3d::ui32 maximumObjectCount = 100000;
	constexpr dx3d::ui32 maximumStringLength = 1024 * 1024;
	constexpr dx3d::ui32 maximumVertexCount = 10000000;
	constexpr dx3d::ui32 maximumIndexCount = 30000000;
	constexpr char jsonSceneFormat[] = "jnpf.scene";
	constexpr char jsonPayloadEncoding[] = "hex";

	std::string encodeHex(const std::string& bytes)
	{
		constexpr char digits[] = "0123456789abcdef";
		std::string encoded{};
		encoded.resize(bytes.size() * 2);
		for (size_t index = 0; index < bytes.size(); ++index)
		{
			const auto value = static_cast<unsigned char>(bytes[index]);
			encoded[index * 2] = digits[value >> 4];
			encoded[index * 2 + 1] = digits[value & 0x0f];
		}
		return encoded;
	}

	bool decodeHex(const std::string& encoded, std::string& bytes)
	{
		if ((encoded.size() & 1u) != 0u)
			return false;

		auto valueOf = [](char value) -> int
		{
			if (value >= '0' && value <= '9') return value - '0';
			if (value >= 'a' && value <= 'f') return value - 'a' + 10;
			if (value >= 'A' && value <= 'F') return value - 'A' + 10;
			return -1;
		};

		bytes.resize(encoded.size() / 2);
		for (size_t index = 0; index < bytes.size(); ++index)
		{
			const int high = valueOf(encoded[index * 2]);
			const int low = valueOf(encoded[index * 2 + 1]);
			if (high < 0 || low < 0)
			{
				bytes.clear();
				return false;
			}
			bytes[index] = static_cast<char>((high << 4) | low);
		}
		return true;
	}

	bool readJsonStringField(
		const std::string& document,
		const std::string& key,
		std::string& value)
	{
		const std::string marker = "\"" + key + "\"";
		const size_t keyPosition = document.find(marker);
		if (keyPosition == std::string::npos) return false;
		const size_t colon = document.find(':', keyPosition + marker.size());
		if (colon == std::string::npos) return false;
		const size_t openingQuote = document.find('"', colon + 1);
		if (openingQuote == std::string::npos) return false;
		const size_t closingQuote = document.find('"', openingQuote + 1);
		if (closingQuote == std::string::npos) return false;
		value = document.substr(openingQuote + 1, closingQuote - openingQuote - 1);
		return true;
	}

	bool unwrapJsonScene(const std::string& document, std::string& binaryScene)
	{
		std::string format{};
		std::string encoding{};
		std::string payload{};
		return readJsonStringField(document, "format", format) &&
			format == jsonSceneFormat &&
			readJsonStringField(document, "encoding", encoding) &&
			encoding == jsonPayloadEncoding &&
			readJsonStringField(document, "payload", payload) &&
			decodeHex(payload, binaryScene);
	}

	bool writeJsonSceneFile(
		const std::string& binaryScene,
		const std::string& filePath)
	{
		if (binaryScene.empty()) return false;

		std::ofstream stream(
			filePath,
			std::ios::trunc
		);

		if (!stream)
		{
			return false;
		}

		// Scene and prefab files are valid, human-identifiable JSON. The
		// versioned binary payload remains the single source of truth for
		// undo/play snapshots and lets existing binary scene files remain loadable.
		stream << "{\n"
			<< "  \"format\": \"" << jsonSceneFormat << "\",\n"
			<< "  \"version\": 1,\n"
			<< "  \"encoding\": \"" << jsonPayloadEncoding << "\",\n"
			<< "  \"payload\": \"" << encodeHex(binaryScene) << "\"\n"
			<< "}\n";

		return static_cast<bool>(stream);
	}

	enum class SceneObjectType : dx3d::ui32
	{
		Empty = 0,
		Cube = 1,
		Plane = 2,
		CombinedMesh = 3,
		DirectionalLight = 4,
		Sphere = 5,
		Cylinder = 6,
		Capsule = 7
	};

	struct SerializedSceneObject
	{
		dx3d::ui64 entityId{};
		dx3d::ui64 parentEntityId{};
		SceneObjectType type{};
		std::string name{};
		bool activeSelf{ true };

		dx3d::Vec3 position{};
		dx3d::Vec3 rotation{};
		dx3d::Vec4 rotationQuaternion{ 0.0f, 0.0f, 0.0f, 1.0f };
		bool hasRotationQuaternion{};

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
		dx3d::LightType lightType{ dx3d::LightType::Directional };
		dx3d::f32 lightRange{ 10.0f };
		dx3d::f32 spotAngle{ 45.0f };
		bool hasMaterial{};
		dx3d::MaterialMode materialMode{ dx3d::MaterialMode::LitTint };
		dx3d::Vec4 materialAlbedo{ 1.0f, 1.0f, 1.0f, 1.0f };
		dx3d::Vec3 materialEmissive{};
		dx3d::f32 materialEmissionStrength{};
		bool hasTexture{};
		std::string textureAssetPath{};
		bool textureEnabled{ true };
		bool hasRigidBody{};
		dx3d::RigidBodyType rigidBodyType{ dx3d::RigidBodyType::Static };
		dx3d::f32 rigidBodyFriction{ 0.5f };
		dx3d::f32 rigidBodyRestitution{};
		dx3d::f32 rigidBodyLinearDamping{ 0.05f };
		dx3d::f32 rigidBodyAngularDamping{ 0.05f };
		dx3d::f32 rigidBodyGravityFactor{ 1.0f };
		bool rigidBodyEnabled{ true };
		bool hasCollider{};
		dx3d::ColliderShape colliderShape{ dx3d::ColliderShape::Box };
		dx3d::Vec3 colliderHalfExtents{ 0.5f, 0.5f, 0.5f };
		dx3d::f32 colliderRadius{ 0.5f };
		bool hasRotator{};
		dx3d::Vec3 rotatorAngularVelocity{ 0.0f, 1.0f, 0.0f };
		bool rotatorEnabled{ true };
		bool hasFlyController{};
		dx3d::f32 flyMoveSpeed{ 5.0f };
		dx3d::f32 flyLookSensitivity{ 0.0025f };
		dx3d::f32 flyBoostMultiplier{ 4.0f };
		dx3d::f32 flyPitchLimitDegrees{ 89.0f };
		bool flyEnabled{ true };
		bool hasCamera{};
		dx3d::f32 cameraNearPlane{ 0.01f };
		dx3d::f32 cameraFarPlane{ 100.0f };
		dx3d::f32 cameraFieldOfView{ 1.3f };
		dx3d::f32 cameraAspectRatio{ 16.0f / 9.0f };
		bool cameraPrimary{};
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

		// The editor camera belongs to the host, not to the authored scene.
		if (object->getName() == "Editor Camera")
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
			dx3d::SphereComponent
			>()
			)
		{
			type = SceneObjectType::Sphere;
			return true;
		}

		if (object->getComponent<dx3d::CylinderComponent>())
		{
			type = SceneObjectType::Cylinder;
			return true;
		}

		if (object->getComponent<dx3d::CapsuleComponent>())
		{
			type = SceneObjectType::Capsule;
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

		type = SceneObjectType::Empty;
		return true;
	}

	bool writeSceneObject(
		std::ostream& stream,
		dx3d::GameObject* object,
		SceneObjectType type,
		const std::unordered_set<dx3d::ui64>* scopedEntityIds = nullptr
	)
	{
		dx3d::ui64 parentEntityId = 0;
		if (auto* parent = object->getParent())
		{
			SceneObjectType parentType{};
			if (getObjectType(parent, parentType) &&
				(!scopedEntityIds || scopedEntityIds->contains(parent->getEntityId())))
				parentEntityId = parent->getEntityId();
		}

		const auto storedType =
			static_cast<dx3d::ui32>(
				type
				);

		auto* material = object->getComponent<dx3d::MaterialComponent>();
		const dx3d::ui32 hasMaterial = material ? 1u : 0u;
		const dx3d::ui32 activeSelf = object->isActiveSelf() ? 1u : 0u;

		if (
			!writeValue(stream, object->getEntityId()) ||
			!writeValue(stream, parentEntityId) ||
			!writeValue(stream, activeSelf) ||
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

		auto* texture = object->getComponent<dx3d::TextureComponent>();
		const dx3d::ui32 hasTexture = texture ? 1u : 0u;
		if (!writeValue(stream, hasTexture)) return false;
		if (texture)
		{
			const dx3d::ui32 enabled = texture->isEnabled() ? 1u : 0u;
			if (!writeString(stream, texture->getAssetPath()) ||
				!writeValue(stream, enabled)) return false;
		}

		auto* rigidBody = object->getComponent<dx3d::RigidBodyComponent>();
		const dx3d::ui32 hasRigidBody = rigidBody ? 1u : 0u;
		if (!writeValue(stream, hasRigidBody)) return false;
		if (rigidBody)
		{
			const auto bodyType = static_cast<dx3d::ui32>(rigidBody->getBodyType());
			const dx3d::ui32 enabled = rigidBody->isEnabled() ? 1u : 0u;
			if (!writeValue(stream, bodyType) ||
				!writeValue(stream, rigidBody->getFriction()) ||
				!writeValue(stream, rigidBody->getRestitution()) ||
				!writeValue(stream, rigidBody->getLinearDamping()) ||
				!writeValue(stream, rigidBody->getAngularDamping()) ||
				!writeValue(stream, rigidBody->getGravityFactor()) ||
				!writeValue(stream, enabled))
			{
				return false;
			}
		}

		auto* collider = object->getComponent<dx3d::ColliderComponent>();
		const dx3d::ui32 hasCollider = collider ? 1u : 0u;
		if (!writeValue(stream, hasCollider)) return false;
		if (collider)
		{
			const auto shape = static_cast<dx3d::ui32>(collider->getShape());
			if (!writeValue(stream, shape) ||
				!writeVec3(stream, collider->getHalfExtents()) ||
				!writeValue(stream, collider->getRadius())) return false;
		}

		auto* rotator = object->getComponent<dx3d::RotatorComponent>();
		const dx3d::ui32 hasRotator = rotator ? 1u : 0u;
		if (!writeValue(stream, hasRotator)) return false;
		if (rotator)
		{
			const dx3d::ui32 enabled = rotator->isEnabled() ? 1u : 0u;
			if (!writeVec3(stream, rotator->getAngularVelocity()) ||
				!writeValue(stream, enabled)) return false;
		}

		auto* fly = object->getComponent<dx3d::FlyControllerComponent>();
		const dx3d::ui32 hasFly = fly ? 1u : 0u;
		if (!writeValue(stream, hasFly)) return false;
		if (fly)
		{
			const dx3d::ui32 enabled = fly->isEnabled() ? 1u : 0u;
			if (!writeValue(stream, fly->getMoveSpeed()) ||
				!writeValue(stream, fly->getLookSensitivity()) ||
				!writeValue(stream, fly->getBoostMultiplier()) ||
				!writeValue(stream, fly->getPitchLimitDegrees()) ||
				!writeValue(stream, enabled)) return false;
		}

		auto* camera = object->getComponent<dx3d::CameraComponent>();
		const dx3d::ui32 hasCamera = camera ? 1u : 0u;
		if (!writeValue(stream, hasCamera)) return false;
		if (camera)
		{
			const dx3d::ui32 primary = camera->isPrimary() ? 1u : 0u;
			if (
			(!writeValue(stream, camera->getNearPlane()) ||
			 !writeValue(stream, camera->getFarPlane()) ||
			 !writeValue(stream, camera->getFieldOfView()) ||
			 !writeValue(stream, camera->getAspectRatio()) ||
			 !writeValue(stream, primary))) return false;
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
			!writeVec4(stream, transform.getRotationQuaternion()) ||
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
		case SceneObjectType::Empty:
		case SceneObjectType::Cube:
		case SceneObjectType::Sphere:
		case SceneObjectType::Cylinder:
		case SceneObjectType::Capsule:
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
			const dx3d::ui32 lightType = static_cast<dx3d::ui32>(light->getLightType());

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
				) &&
				writeValue(stream, lightType) &&
				writeValue(stream, light->getRange()) &&
				writeValue(stream, light->getSpotAngle());
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
		if (storedVersion >= 8)
		{
			dx3d::ui32 activeSelf = 0;
			if (!readValue(stream, activeSelf) || activeSelf > 1u)
				return false;
			object.activeSelf = activeSelf != 0;
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

		if (storedVersion >= 10)
		{
			dx3d::ui32 hasTexture = 0;
			if (!readValue(stream, hasTexture) || hasTexture > 1u) return false;
			object.hasTexture = hasTexture != 0;
			if (object.hasTexture)
			{
				dx3d::ui32 enabled = 0;
				if (!readString(stream, object.textureAssetPath) ||
					!readValue(stream, enabled) || enabled > 1u) return false;
				object.textureEnabled = enabled != 0;
			}
		}

		if (storedVersion == 4)
		{
			dx3d::ui32 hasRigidBody = 0;
			if (!readValue(stream, hasRigidBody) || hasRigidBody > 1u) return false;
			object.hasRigidBody = hasRigidBody != 0;
			if (object.hasRigidBody)
			{
				dx3d::ui32 bodyType = 0;
				dx3d::ui32 shape = 0;
				dx3d::ui32 gravity = 0;
				dx3d::f32 ignoredMass = 1.0f;
				if (!readValue(stream, bodyType) || bodyType > 2u ||
					!readValue(stream, shape) || shape > 1u ||
					!readVec3(stream, object.colliderHalfExtents) ||
					!readValue(stream, object.colliderRadius) ||
					!readValue(stream, ignoredMass) ||
					!readValue(stream, object.rigidBodyRestitution) ||
					!readValue(stream, gravity) || gravity > 1u)
				{
					return false;
				}
				object.rigidBodyType = static_cast<dx3d::RigidBodyType>(bodyType);
				object.colliderShape = static_cast<dx3d::ColliderShape>(shape);
				object.rigidBodyGravityFactor = gravity != 0 ? 1.0f : 0.0f;
				object.hasCollider = true;
			}
		}
		else if (storedVersion >= 5)
		{
			dx3d::ui32 hasRigidBody = 0;
			if (!readValue(stream, hasRigidBody) || hasRigidBody > 1u) return false;
			object.hasRigidBody = hasRigidBody != 0;
			if (object.hasRigidBody)
			{
				dx3d::ui32 bodyType = 0;
				dx3d::ui32 enabled = 0;
				if (!readValue(stream, bodyType) || bodyType > 2u ||
					!readValue(stream, object.rigidBodyFriction) ||
					!readValue(stream, object.rigidBodyRestitution) ||
					!readValue(stream, object.rigidBodyLinearDamping) ||
					!readValue(stream, object.rigidBodyAngularDamping) ||
					!readValue(stream, object.rigidBodyGravityFactor) ||
					!readValue(stream, enabled) || enabled > 1u) return false;
				object.rigidBodyType = static_cast<dx3d::RigidBodyType>(bodyType);
				object.rigidBodyEnabled = enabled != 0;
			}

			dx3d::ui32 hasCollider = 0;
			if (!readValue(stream, hasCollider) || hasCollider > 1u) return false;
			object.hasCollider = hasCollider != 0;
			if (object.hasCollider)
			{
				dx3d::ui32 shape = 0;
				const dx3d::ui32 maximumShape = storedVersion >= 11 ? 3u : 1u;
				if (!readValue(stream, shape) || shape > maximumShape ||
					!readVec3(stream, object.colliderHalfExtents) ||
					!readValue(stream, object.colliderRadius)) return false;
				object.colliderShape = static_cast<dx3d::ColliderShape>(shape);
			}

			if (storedVersion >= 6)
			{
				dx3d::ui32 hasRotator = 0;
				if (!readValue(stream, hasRotator) || hasRotator > 1u) return false;
				object.hasRotator = hasRotator != 0;
				if (object.hasRotator)
				{
					dx3d::ui32 enabled = 0;
					if (!readVec3(stream, object.rotatorAngularVelocity) ||
						!readValue(stream, enabled) || enabled > 1u) return false;
					object.rotatorEnabled = enabled != 0;
				}

				dx3d::ui32 hasFly = 0;
				if (!readValue(stream, hasFly) || hasFly > 1u) return false;
				object.hasFlyController = hasFly != 0;
				if (object.hasFlyController)
				{
					dx3d::ui32 enabled = 0;
					if (!readValue(stream, object.flyMoveSpeed) ||
						!readValue(stream, object.flyLookSensitivity) ||
						!readValue(stream, object.flyBoostMultiplier) ||
						!readValue(stream, object.flyPitchLimitDegrees) ||
						!readValue(stream, enabled) || enabled > 1u) return false;
					object.flyEnabled = enabled != 0;
				}

				dx3d::ui32 hasCamera = 0;
				if (!readValue(stream, hasCamera) || hasCamera > 1u) return false;
				object.hasCamera = hasCamera != 0;
				if (object.hasCamera &&
					(!readValue(stream, object.cameraNearPlane) ||
					 !readValue(stream, object.cameraFarPlane) ||
					 !readValue(stream, object.cameraFieldOfView))) return false;
				if (object.hasCamera && storedVersion >= 7)
				{
					dx3d::ui32 primary = 0;
					if (!readValue(stream, object.cameraAspectRatio) ||
						!readValue(stream, primary) || primary > 1u) return false;
					object.cameraPrimary = primary != 0;
				}
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
		case SceneObjectType::Empty:
		case SceneObjectType::Cube:
		case SceneObjectType::Sphere:
		case SceneObjectType::Cylinder:
		case SceneObjectType::Capsule:
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
			(storedVersion >= 5 && !readVec4(stream, object.rotationQuaternion)) ||
			!readVec3(
				stream,
				object.scale
			)
			)
		{
			return false;
		}
		object.hasRotationQuaternion = storedVersion >= 5;

		switch (object.type)
		{
		case SceneObjectType::Empty:
		case SceneObjectType::Cube:
		case SceneObjectType::Sphere:
		case SceneObjectType::Cylinder:
		case SceneObjectType::Capsule:
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
			if (storedVersion >= 12)
			{
				dx3d::ui32 lightType = 0;
				if (!readValue(stream, lightType) || lightType > 2u ||
					!readValue(stream, object.lightRange) ||
					!readValue(stream, object.spotAngle))
				{
					return false;
				}
				object.lightType = static_cast<dx3d::LightType>(lightType);
			}

			return true;
		}
		}

		return false;
	}

	dx3d::GameObject* instantiateObject(
		dx3d::World& world,
		const SerializedSceneObject& data,
		dx3d::SceneLoadResult& result,
		bool preserveEntityId = true
	)
	{
		auto* object = world.createGameObjectWithId<
			dx3d::GameObject>(preserveEntityId ? data.entityId : 0);

		if (!object)
		{
			return nullptr;
		}

		object->setName(
			data.name
		);
		object->setActive(data.activeSelf);

		switch (data.type)
		{
		case SceneObjectType::Empty:
			break;
		case SceneObjectType::Cube:
			object->createOrGetComponent<
				dx3d::CubeComponent
			>();

			++result.cubeCount;
			break;

		case SceneObjectType::Sphere:
			object->createOrGetComponent<dx3d::SphereComponent>();
			break;

		case SceneObjectType::Cylinder:
			object->createOrGetComponent<dx3d::CylinderComponent>();
			break;

		case SceneObjectType::Capsule:
			object->createOrGetComponent<dx3d::CapsuleComponent>();
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

			light->setLightType(data.lightType);
			light->setRange(data.lightRange);
			light->setSpotAngle(data.spotAngle);

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
		if (data.hasRotationQuaternion)
			transform.setRotationQuaternion(data.rotationQuaternion, data.rotation);

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
		if (data.hasTexture)
		{
			auto* texture = object->createOrGetComponent<dx3d::TextureComponent>();
			texture->setAssetPath(data.textureAssetPath);
			texture->setEnabled(data.textureEnabled);
		}

		if (data.hasRigidBody)
		{
			auto* rigidBody = object->createOrGetComponent<dx3d::RigidBodyComponent>();
			rigidBody->setBodyType(data.rigidBodyType);
			rigidBody->setFriction(data.rigidBodyFriction);
			rigidBody->setRestitution(data.rigidBodyRestitution);
			rigidBody->setLinearDamping(data.rigidBodyLinearDamping);
			rigidBody->setAngularDamping(data.rigidBodyAngularDamping);
			rigidBody->setGravityFactor(data.rigidBodyGravityFactor);
			rigidBody->setEnabled(data.rigidBodyEnabled);
		}
		if (data.hasCollider)
		{
			auto* collider = object->createOrGetComponent<dx3d::ColliderComponent>();
			collider->setShape(data.colliderShape);
			collider->setHalfExtents(data.colliderHalfExtents);
			collider->setRadius(data.colliderRadius);
		}
		if (data.hasRotator)
		{
			auto* rotator = object->createOrGetComponent<dx3d::RotatorComponent>();
			rotator->setAngularVelocity(data.rotatorAngularVelocity);
			rotator->setEnabled(data.rotatorEnabled);
		}
		if (data.hasFlyController)
		{
			auto* fly = object->createOrGetComponent<dx3d::FlyControllerComponent>();
			fly->setMoveSpeed(data.flyMoveSpeed);
			fly->setLookSensitivity(data.flyLookSensitivity);
			fly->setBoostMultiplier(data.flyBoostMultiplier);
			fly->setPitchLimitDegrees(data.flyPitchLimitDegrees);
			fly->setEnabled(data.flyEnabled);
		}
		if (data.hasCamera)
		{
			auto* camera = object->createOrGetComponent<dx3d::CameraComponent>();
			camera->setNearPlane(data.cameraNearPlane);
			camera->setFarPlane(data.cameraFarPlane);
			camera->setFieldOfView(data.cameraFieldOfView);
			camera->setAspectRatio(data.cameraAspectRatio);
			camera->setPrimary(data.cameraPrimary);
		}

		return object;
	}

	void collectSerializableSubtree(
		dx3d::GameObject* object,
		std::vector<std::pair<dx3d::GameObject*, SceneObjectType>>& objects)
	{
		if (!object) return;

		SceneObjectType type{};
		if (getObjectType(object, type))
			objects.push_back({ object, type });

		for (auto* child : object->getChildren())
			collectSerializableSubtree(child, objects);
	}

	std::string serializeObjects(
		const std::vector<std::pair<dx3d::GameObject*, SceneObjectType>>& objectsToSave,
		bool scopedParentLinks)
	{
		if (objectsToSave.size() > maximumObjectCount)
		{
			return {};
		}

		std::unordered_set<dx3d::ui64> scopedEntityIds{};
		if (scopedParentLinks)
		{
			scopedEntityIds.reserve(objectsToSave.size());
			for (const auto& [object, type] : objectsToSave)
				if (object) scopedEntityIds.insert(object->getEntityId());
		}

		std::ostringstream stream(
			std::ios::out | std::ios::binary
		);

		stream.write(sceneMagic, sizeof(sceneMagic));

		const auto objectCount =
			static_cast<dx3d::ui32>(objectsToSave.size());

		if (!stream ||
			!writeValue(stream, sceneVersion) ||
			!writeValue(stream, objectCount))
		{
			return {};
		}

		for (const auto& [object, type] : objectsToSave)
		{
			if (!writeSceneObject(
				stream,
				object,
				type,
				scopedParentLinks ? &scopedEntityIds : nullptr))
			{
				return {};
			}
		}

		return stream.str();
	}

	bool readSceneFile(
		const std::string& filePath,
		std::string& sceneData)
	{
		std::ifstream stream(
			filePath,
			std::ios::binary
		);

		if (!stream) return false;

		std::ostringstream contents{};
		contents << stream.rdbuf();
		if (!stream && !stream.eof()) return false;

		sceneData = contents.str();
		if (sceneData.size() < sizeof(sceneMagic) ||
			sceneData.compare(0, sizeof(sceneMagic), sceneMagic, sizeof(sceneMagic)) != 0)
		{
			std::string binaryScene{};
			if (!unwrapJsonScene(sceneData, binaryScene)) return false;
			sceneData = std::move(binaryScene);
		}

		return true;
	}

	bool readSerializedObjects(
		const std::string& sceneData,
		std::vector<SerializedSceneObject>& serializedObjects)
	{
		if (sceneData.empty())
		{
			return false;
		}

		std::istringstream stream(
			sceneData,
			std::ios::in | std::ios::binary
		);

		char storedMagic[sizeof(sceneMagic)]{};
		stream.read(storedMagic, sizeof(storedMagic));

		if (!stream)
		{
			return false;
		}

		for (size_t index = 0; index < sizeof(sceneMagic); ++index)
		{
			if (storedMagic[index] != sceneMagic[index])
			{
				return false;
			}
		}

		dx3d::ui32 storedVersion = 0;
		dx3d::ui32 objectCount = 0;

		if (!readValue(stream, storedVersion) ||
			!readValue(stream, objectCount) ||
			storedVersion < oldestSupportedSceneVersion ||
			storedVersion > sceneVersion ||
			objectCount > maximumObjectCount)
		{
			return false;
		}

		serializedObjects.resize(objectCount);

		for (auto& object : serializedObjects)
		{
			if (!readSceneObject(stream, object, storedVersion))
			{
				serializedObjects.clear();
				return false;
			}
		}

		return true;
	}

	dx3d::GameObject* appendPrefabObjects(
		dx3d::World& world,
		const std::vector<SerializedSceneObject>& serializedObjects,
		dx3d::GameObject* parent)
	{
		if (serializedObjects.empty()) return nullptr;

		std::unordered_set<dx3d::ui64> sourceEntityIds{};
		sourceEntityIds.reserve(serializedObjects.size());
		for (const auto& object : serializedObjects)
			if (object.entityId != 0) sourceEntityIds.insert(object.entityId);

		std::vector<dx3d::ui64> rootIds{};
		for (const auto& object : serializedObjects)
		{
			if (object.entityId == 0) return nullptr;
			if (object.parentEntityId == 0 ||
				!sourceEntityIds.contains(object.parentEntityId))
			{
				rootIds.push_back(object.entityId);
			}
		}
		if (rootIds.size() != 1) return nullptr;

		dx3d::SceneLoadResult result{};
		std::unordered_map<dx3d::ui64, dx3d::GameObject*> loadedEntities{};
		loadedEntities.reserve(serializedObjects.size());
		for (const auto& object : serializedObjects)
		{
			auto* loaded = instantiateObject(world, object, result, false);
			if (!loaded) return nullptr;
			loadedEntities[object.entityId] = loaded;
		}

		for (const auto& object : serializedObjects)
		{
			if (object.parentEntityId == 0) continue;

			const auto child = loadedEntities.find(object.entityId);
			const auto loadedParent = loadedEntities.find(object.parentEntityId);
			if (child != loadedEntities.end() && loadedParent != loadedEntities.end())
				world.setParent(child->second, loadedParent->second);
		}

		auto* root = loadedEntities[rootIds.front()];
		if (parent && !world.setParent(root, parent))
			return nullptr;
		return root;
	}
}

bool dx3d::SceneSerializer::save(
	World& world,
	const std::string& filePath
)
{
	const std::string binaryScene = serialize(world);
	return writeJsonSceneFile(binaryScene, filePath);
}

bool dx3d::SceneSerializer::savePrefab(
	GameObject& root,
	const std::string& filePath
)
{
	SceneObjectType rootType{};
	if (!getObjectType(&root, rootType)) return false;

	std::vector<std::pair<GameObject*, SceneObjectType>> objectsToSave{};
	collectSerializableSubtree(&root, objectsToSave);
	if (objectsToSave.empty() || objectsToSave.front().first != &root)
		return false;

	const std::string binaryScene = serializeObjects(objectsToSave, true);
	return writeJsonSceneFile(binaryScene, filePath);
}

dx3d::GameObject* dx3d::SceneSerializer::instantiatePrefab(
	World& world,
	const std::string& filePath,
	GameObject* parent
)
{
	std::string sceneData{};
	if (!readSceneFile(filePath, sceneData)) return nullptr;

	std::vector<SerializedSceneObject> serializedObjects{};
	if (!readSerializedObjects(sceneData, serializedObjects)) return nullptr;

	return appendPrefabObjects(world, serializedObjects, parent);
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

	return serializeObjects(objectsToSave, false);
}

dx3d::SceneLoadResult
dx3d::SceneSerializer::load(
	World& world,
	const std::string& filePath
)
{
	std::string sceneData{};
	if (!readSceneFile(filePath, sceneData)) return {};

	return deserialize(world, sceneData);
}

dx3d::SceneLoadResult
dx3d::SceneSerializer::deserialize(
	World& world,
	const std::string& sceneData
)
{
	SceneLoadResult result{};

	std::vector<SerializedSceneObject> serializedObjects{};
	if (!readSerializedObjects(sceneData, serializedObjects)) return result;

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

		if (object->getName() == "Editor Camera")
		{
			continue;
		}

		world.destroyGameObject(
			object
		);
	}
}
