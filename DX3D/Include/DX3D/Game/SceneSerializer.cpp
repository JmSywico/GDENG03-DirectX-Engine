#include <DX3D/Game/SceneSerializer.h>

#include <DX3D/Game/World.h>
#include <DX3D/Game/GameObject.h>

#include <DX3D/Component/CameraComponent.h>
#include <DX3D/Component/CubeComponent.h>
#include <DX3D/Component/PlaneComponent.h>
#include <DX3D/Component/SphereComponent.h>
#include <DX3D/Component/CapsuleComponent.h>
#include <DX3D/Component/CombinedMeshComponent.h>
#include <DX3D/Component/DirectionalLightComponent.h>
#include <DX3D/Component/MaterialComponent.h>
#include <DX3D/Component/RigidBodyComponent.h>
#include <DX3D/Component/TransformComponent.h>

#include <DX3D/Graphics/MeshData.h>
#include <DX3D/Math/Vec2.h>
#include <DX3D/Math/Vec3.h>
#include <DX3D/Math/Vec4.h>

#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <type_traits>
#include <utility>
#include <cstddef>
#include <sstream>
#include <iomanip>
#include <iterator>
#include <cctype>
#include <cstdlib>

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

	constexpr dx3d::ui32 sceneVersion = 5;
	constexpr dx3d::ui32 minimumSupportedSceneVersion = 1;
	constexpr dx3d::ui32 maximumObjectCount = 100000;
	constexpr dx3d::ui32 maximumLevelObjectCount = 10000;
	constexpr dx3d::ui32 maximumStringLength = 1024 * 1024;
	constexpr dx3d::ui32 maximumVertexCount = 10000000;
	constexpr dx3d::ui32 maximumIndexCount = 30000000;

	enum class SceneObjectType : dx3d::ui32
	{
		Cube = 1,
		Plane = 2,
		CombinedMesh = 3,
		DirectionalLight = 4,
		Sphere = 5,
		Capsule = 6
	};

	struct SerializedSceneObject
	{
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

		bool hasRigidBody{ false };
		dx3d::Vec3 rigidBodyVelocity{};
		dx3d::Vec3 rigidBodyAngularVelocity{};
		dx3d::f32 rigidBodyMass{ 1.0f };
		dx3d::f32 rigidBodyRestitution{ 0.45f };
		dx3d::f32 rigidBodyFriction{ 0.20f };
		bool rigidBodyUseGravity{ true };
		bool rigidBodyIsStatic{ false };
		dx3d::Vec3 rigidBodyColliderSize
		{
			1.0f,
			1.0f,
			1.0f
		};
		dx3d::Vec3 rigidBodyColliderOffset{};
		bool rigidBodyColliderUsesTransformScale{ true };

		bool hasMaterial{ false };
		std::string materialTexturePath{};
		dx3d::Vec2 materialUvTiling
		{
			1.0f,
			1.0f
		};
		dx3d::Vec2 materialUvOffset{};
		dx3d::Vec4 materialColor
		{
			1.0f,
			1.0f,
			1.0f,
			1.0f
		};

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
	};

	struct JsonValue
	{
		enum class Type
		{
			Null,
			Bool,
			Number,
			String,
			Array,
			Object
		};

		Type type{ Type::Null };
		bool boolValue{};
		double numberValue{};
		std::string stringValue{};
		std::vector<JsonValue> arrayValues{};
		std::unordered_map<
			std::string,
			JsonValue
		> objectValues{};
	};

	class JsonParser final
	{
	public:
		explicit JsonParser(
			const std::string& text
		) noexcept
			: m_text(text)
		{}

		bool parse(
			JsonValue& value
		)
		{
			skipWhitespace();

			if (!parseValue(value))
				return false;

			skipWhitespace();

			return m_position ==
				m_text.size();
		}

	private:
		void skipWhitespace() noexcept
		{
			while (m_position < m_text.size() &&
				std::isspace(
					static_cast<unsigned char>(
						m_text[m_position]
					)
				))
			{
				++m_position;
			}
		}

		bool consume(char expected) noexcept
		{
			skipWhitespace();

			if (m_position >= m_text.size() ||
				m_text[m_position] != expected)
			{
				return false;
			}

			++m_position;
			return true;
		}

		bool matchLiteral(
			const char* literal
		) noexcept
		{
			size_t offset = 0;

			while (literal[offset] != '\0')
			{
				if (m_position + offset >=
					m_text.size() ||
					m_text[m_position + offset] !=
					literal[offset])
				{
					return false;
				}

				++offset;
			}

			m_position += offset;
			return true;
		}

		bool parseValue(
			JsonValue& value
		)
		{
			skipWhitespace();

			if (m_position >= m_text.size())
				return false;

			const char current =
				m_text[m_position];

			if (current == '"')
				return parseString(value);

			if (current == '{')
				return parseObject(value);

			if (current == '[')
				return parseArray(value);

			if (current == '-' ||
				std::isdigit(
					static_cast<unsigned char>(
						current
					)
				))
			{
				return parseNumber(value);
			}

			if (matchLiteral("true"))
			{
				value = {};
				value.type = JsonValue::Type::Bool;
				value.boolValue = true;
				return true;
			}

			if (matchLiteral("false"))
			{
				value = {};
				value.type = JsonValue::Type::Bool;
				value.boolValue = false;
				return true;
			}

			if (matchLiteral("null"))
			{
				value = {};
				return true;
			}

			return false;
		}

		bool parseString(
			JsonValue& value
		)
		{
			if (!consume('"'))
				return false;

			std::string parsed{};

			while (m_position < m_text.size())
			{
				const char current =
					m_text[m_position++];

				if (current == '"')
				{
					value = {};
					value.type =
						JsonValue::Type::String;
					value.stringValue =
						std::move(parsed);

					return true;
				}

				if (current != '\\')
				{
					parsed.push_back(current);
					continue;
				}

				if (m_position >= m_text.size())
					return false;

				const char escaped =
					m_text[m_position++];

				switch (escaped)
				{
				case '"':
				case '\\':
				case '/':
					parsed.push_back(escaped);
					break;

				case 'b':
					parsed.push_back('\b');
					break;

				case 'f':
					parsed.push_back('\f');
					break;

				case 'n':
					parsed.push_back('\n');
					break;

				case 'r':
					parsed.push_back('\r');
					break;

				case 't':
					parsed.push_back('\t');
					break;

				case 'u':
					if (m_position + 4 >
						m_text.size())
					{
						return false;
					}

					m_position += 4;
					parsed.push_back('?');
					break;

				default:
					return false;
				}
			}

			return false;
		}

		bool parseNumber(
			JsonValue& value
		)
		{
			const char* textStart =
				m_text.c_str();

			char* numberEnd = nullptr;

			const double parsed =
				std::strtod(
					textStart + m_position,
					&numberEnd
				);

			if (numberEnd ==
				textStart + m_position)
			{
				return false;
			}

			m_position =
				static_cast<size_t>(
					numberEnd - textStart
				);

			value = {};
			value.type =
				JsonValue::Type::Number;
			value.numberValue = parsed;

			return true;
		}

		bool parseArray(
			JsonValue& value
		)
		{
			if (!consume('['))
				return false;

			value = {};
			value.type =
				JsonValue::Type::Array;

			skipWhitespace();

			if (m_position < m_text.size() &&
				m_text[m_position] == ']')
			{
				++m_position;
				return true;
			}

			while (true)
			{
				JsonValue item{};

				if (!parseValue(item))
					return false;

				value.arrayValues.push_back(
					std::move(item)
				);

				skipWhitespace();

				if (m_position >= m_text.size())
					return false;

				if (m_text[m_position] == ']')
				{
					++m_position;
					return true;
				}

				if (m_text[m_position] != ',')
					return false;

				++m_position;
			}
		}

		bool parseObject(
			JsonValue& value
		)
		{
			if (!consume('{'))
				return false;

			value = {};
			value.type =
				JsonValue::Type::Object;

			skipWhitespace();

			if (m_position < m_text.size() &&
				m_text[m_position] == '}')
			{
				++m_position;
				return true;
			}

			while (true)
			{
				JsonValue key{};

				if (!parseString(key))
					return false;

				if (!consume(':'))
					return false;

				JsonValue memberValue{};

				if (!parseValue(memberValue))
					return false;

				value.objectValues.emplace(
					std::move(key.stringValue),
					std::move(memberValue)
				);

				skipWhitespace();

				if (m_position >= m_text.size())
					return false;

				if (m_text[m_position] == '}')
				{
					++m_position;
					return true;
				}

				if (m_text[m_position] != ',')
					return false;

				++m_position;
			}
		}

	private:
		const std::string& m_text;
		size_t m_position{};
	};

	template <typename T>
	bool writeValue(
		std::ofstream& stream,
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
		std::ifstream& stream,
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
		std::ofstream& stream,
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
		std::ifstream& stream,
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
		std::ofstream& stream,
		const dx3d::Vec3& value
	)
	{
		return
			writeValue(stream, value.x) &&
			writeValue(stream, value.y) &&
			writeValue(stream, value.z);
	}

	bool readVec3(
		std::ifstream& stream,
		dx3d::Vec3& value
	)
	{
		return
			readValue(stream, value.x) &&
			readValue(stream, value.y) &&
			readValue(stream, value.z);
	}

	bool writeVec2(
		std::ofstream& stream,
		const dx3d::Vec2& value
	)
	{
		return
			writeValue(stream, value.x) &&
			writeValue(stream, value.y);
	}

	bool readVec2(
		std::ifstream& stream,
		dx3d::Vec2& value
	)
	{
		return
			readValue(stream, value.x) &&
			readValue(stream, value.y);
	}

	bool writeBool(
		std::ofstream& stream,
		bool value
	)
	{
		const dx3d::ui32 storedValue =
			value ? 1u : 0u;

		return writeValue(
			stream,
			storedValue
		);
	}

	bool readBool(
		std::ifstream& stream,
		bool& value
	)
	{
		dx3d::ui32 storedValue = 0;

		if (!readValue(
			stream,
			storedValue
		))
		{
			return false;
		}

		value = storedValue != 0;

		return true;
	}

	bool writeVec4(
		std::ofstream& stream,
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
		std::ifstream& stream,
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
		std::ofstream& stream,
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
				) ||
				!writeVec2(
					stream,
					vertex.texCoord
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
		std::ifstream& stream,
		dx3d::MeshData& meshData,
		dx3d::ui32 storedVersion
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

			if (storedVersion >= 4)
			{
				if (!readVec2(
					stream,
					vertex.texCoord
				))
				{
					return false;
				}
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

	std::string toLowerAscii(
		std::string value
	)
	{
		for (auto& character :
			value)
		{
			character =
				static_cast<char>(
					std::tolower(
						static_cast<unsigned char>(
							character
						)
					)
				);
		}

		return value;
	}

	bool isLevelObjectType(
		SceneObjectType type
	) noexcept
	{
		switch (type)
		{
		case SceneObjectType::Cube:
		case SceneObjectType::Plane:
		case SceneObjectType::Sphere:
		case SceneObjectType::Capsule:
		case SceneObjectType::DirectionalLight:
			return true;

		case SceneObjectType::CombinedMesh:
			return false;
		}

		return false;
	}

	const char* getLevelTypeName(
		SceneObjectType type
	) noexcept
	{
		switch (type)
		{
		case SceneObjectType::Cube:
			return "cube";

		case SceneObjectType::Plane:
			return "plane";

		case SceneObjectType::Sphere:
			return "sphere";

		case SceneObjectType::Capsule:
			return "capsule";

		case SceneObjectType::DirectionalLight:
			return "directionalLight";

		case SceneObjectType::CombinedMesh:
			return "mesh";
		}

		return "unknown";
	}

	bool getLevelTypeFromName(
		const std::string& typeName,
		SceneObjectType& type
	)
	{
		const std::string normalized =
			toLowerAscii(typeName);

		if (normalized == "cube")
		{
			type = SceneObjectType::Cube;
			return true;
		}

		if (normalized == "plane")
		{
			type = SceneObjectType::Plane;
			return true;
		}

		if (normalized == "sphere")
		{
			type = SceneObjectType::Sphere;
			return true;
		}

		if (normalized == "capsule")
		{
			type = SceneObjectType::Capsule;
			return true;
		}

		if (normalized == "directionallight" ||
			normalized == "directional_light" ||
			normalized == "light")
		{
			type =
				SceneObjectType::DirectionalLight;
			return true;
		}

		return false;
	}

	const JsonValue* findMember(
		const JsonValue& object,
		const char* name
	) noexcept
	{
		if (object.type !=
			JsonValue::Type::Object)
		{
			return nullptr;
		}

		const auto iterator =
			object.objectValues.find(name);

		if (iterator ==
			object.objectValues.end())
		{
			return nullptr;
		}

		return &iterator->second;
	}

	bool readStringMember(
		const JsonValue& object,
		const char* name,
		std::string& value
	)
	{
		const auto* member =
			findMember(
				object,
				name
			);

		if (!member ||
			member->type !=
			JsonValue::Type::String)
		{
			return false;
		}

		value = member->stringValue;

		return true;
	}

	bool readNumberMember(
		const JsonValue& object,
		const char* name,
		dx3d::f32& value
	)
	{
		const auto* member =
			findMember(
				object,
				name
			);

		if (!member ||
			member->type !=
			JsonValue::Type::Number)
		{
			return false;
		}

		value =
			static_cast<dx3d::f32>(
				member->numberValue
			);

		return true;
	}

	bool readBoolMember(
		const JsonValue& object,
		const char* name,
		bool& value
	)
	{
		const auto* member =
			findMember(
				object,
				name
			);

		if (!member ||
			member->type !=
			JsonValue::Type::Bool)
		{
			return false;
		}

		value = member->boolValue;

		return true;
	}

	bool readVec3Array(
		const JsonValue& value,
		dx3d::Vec3& output
	)
	{
		if (value.type !=
			JsonValue::Type::Array ||
			value.arrayValues.size() < 3)
		{
			return false;
		}

		for (size_t index = 0;
			index < 3;
			++index)
		{
			if (
				value.arrayValues[index].type !=
				JsonValue::Type::Number
				)
			{
				return false;
			}
		}

		output =
		{
			static_cast<dx3d::f32>(
				value.arrayValues[0].
				numberValue
			),
			static_cast<dx3d::f32>(
				value.arrayValues[1].
				numberValue
			),
			static_cast<dx3d::f32>(
				value.arrayValues[2].
				numberValue
			)
		};

		return true;
	}

	bool readVec2Array(
		const JsonValue& value,
		dx3d::Vec2& output
	)
	{
		if (value.type !=
			JsonValue::Type::Array ||
			value.arrayValues.size() < 2)
		{
			return false;
		}

		for (size_t index = 0;
			index < 2;
			++index)
		{
			if (
				value.arrayValues[index].type !=
				JsonValue::Type::Number
				)
			{
				return false;
			}
		}

		output =
		{
			static_cast<dx3d::f32>(
				value.arrayValues[0].
				numberValue
			),
			static_cast<dx3d::f32>(
				value.arrayValues[1].
				numberValue
			)
		};

		return true;
	}

	bool readVec4Array(
		const JsonValue& value,
		dx3d::Vec4& output
	)
	{
		if (value.type !=
			JsonValue::Type::Array ||
			value.arrayValues.size() < 4)
		{
			return false;
		}

		for (size_t index = 0;
			index < 4;
			++index)
		{
			if (
				value.arrayValues[index].type !=
				JsonValue::Type::Number
				)
			{
				return false;
			}
		}

		output =
		{
			static_cast<dx3d::f32>(
				value.arrayValues[0].
				numberValue
			),
			static_cast<dx3d::f32>(
				value.arrayValues[1].
				numberValue
			),
			static_cast<dx3d::f32>(
				value.arrayValues[2].
				numberValue
			),
			static_cast<dx3d::f32>(
				value.arrayValues[3].
				numberValue
			)
		};

		return true;
	}

	void readVec3Member(
		const JsonValue& object,
		const char* name,
		dx3d::Vec3& output
	)
	{
		const auto* member =
			findMember(
				object,
				name
			);

		if (member)
		{
			readVec3Array(
				*member,
				output
			);
		}
	}

	void readVec2Member(
		const JsonValue& object,
		const char* name,
		dx3d::Vec2& output
	)
	{
		const auto* member =
			findMember(
				object,
				name
			);

		if (member)
		{
			readVec2Array(
				*member,
				output
			);
		}
	}

	void readVec4Member(
		const JsonValue& object,
		const char* name,
		dx3d::Vec4& output
	)
	{
		const auto* member =
			findMember(
				object,
				name
			);

		if (member)
		{
			readVec4Array(
				*member,
				output
			);
		}
	}

	void writeJsonString(
		std::ofstream& stream,
		const std::string& value
	)
	{
		stream << '"';

		for (const char character :
			value)
		{
			switch (character)
			{
			case '"':
				stream << "\\\"";
				break;

			case '\\':
				stream << "\\\\";
				break;

			case '\n':
				stream << "\\n";
				break;

			case '\r':
				stream << "\\r";
				break;

			case '\t':
				stream << "\\t";
				break;

			default:
				stream << character;
				break;
			}
		}

		stream << '"';
	}

	void writeLevelVec3(
		std::ofstream& stream,
		const dx3d::Vec3& value
	)
	{
		stream
			<< "["
			<< value.x
			<< ", "
			<< value.y
			<< ", "
			<< value.z
			<< "]";
	}

	void writeLevelVec2(
		std::ofstream& stream,
		const dx3d::Vec2& value
	)
	{
		stream
			<< "["
			<< value.x
			<< ", "
			<< value.y
			<< "]";
	}

	void writeLevelVec4(
		std::ofstream& stream,
		const dx3d::Vec4& value
	)
	{
		stream
			<< "["
			<< value.x
			<< ", "
			<< value.y
			<< ", "
			<< value.z
			<< ", "
			<< value.w
			<< "]";
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
			dx3d::SphereComponent
			>()
			)
		{
			type = SceneObjectType::Sphere;
			return true;
		}

		if (
			object->getComponent<
			dx3d::CapsuleComponent
			>()
			)
		{
			type = SceneObjectType::Capsule;
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
		std::ofstream& stream,
		dx3d::GameObject* object,
		SceneObjectType type
	)
	{
		const auto storedType =
			static_cast<dx3d::ui32>(
				type
				);

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

		auto* rigidBody =
			object->getComponent<
			dx3d::RigidBodyComponent
			>();

		if (!writeBool(
			stream,
			rigidBody != nullptr
		))
		{
			return false;
		}

		if (rigidBody)
		{
			if (
				!writeVec3(
					stream,
					rigidBody->getVelocity()
				) ||
				!writeVec3(
					stream,
					rigidBody->
					getAngularVelocity()
				) ||
				!writeValue(
					stream,
					rigidBody->getMass()
				) ||
				!writeValue(
					stream,
					rigidBody->
					getRestitution()
				) ||
				!writeValue(
					stream,
					rigidBody->getFriction()
				) ||
				!writeBool(
					stream,
					rigidBody->
					getUseGravity()
				) ||
				!writeBool(
					stream,
					rigidBody->getStatic()
				) ||
				!writeVec3(
					stream,
					rigidBody->
					getColliderSize()
				) ||
				!writeVec3(
					stream,
					rigidBody->
					getColliderOffset()
				) ||
				!writeBool(
					stream,
					rigidBody->
					getColliderUsesTransformScale()
				)
				)
			{
				return false;
			}
		}

		auto* material =
			object->getComponent<
			dx3d::MaterialComponent
			>();

		if (!writeBool(
			stream,
			material != nullptr
		))
		{
			return false;
		}

		if (material)
		{
			if (
				!writeString(
					stream,
					material->getTexturePath()
				) ||
				!writeVec2(
					stream,
					material->getUvTiling()
				) ||
				!writeVec2(
					stream,
					material->getUvOffset()
				) ||
				!writeVec4(
					stream,
					material->getColor()
				)
				)
			{
				return false;
			}
		}

		switch (type)
		{
		case SceneObjectType::Cube:
		case SceneObjectType::Plane:
		case SceneObjectType::Sphere:
		case SceneObjectType::Capsule:
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
		std::ifstream& stream,
		SerializedSceneObject& object,
		dx3d::ui32 storedVersion
	)
	{
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
		case SceneObjectType::Sphere:
		case SceneObjectType::Capsule:
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

		if (storedVersion >= 2)
		{
			if (!readBool(
				stream,
				object.hasRigidBody
			))
			{
				return false;
			}

			if (object.hasRigidBody)
			{
				if (
					!readVec3(
						stream,
						object.
						rigidBodyVelocity
					) ||
					!readVec3(
						stream,
						object.
						rigidBodyAngularVelocity
					) ||
					!readValue(
						stream,
						object.rigidBodyMass
					) ||
					!readValue(
						stream,
						object.
						rigidBodyRestitution
					) ||
					!readValue(
						stream,
						object.rigidBodyFriction
					) ||
					!readBool(
						stream,
						object.
						rigidBodyUseGravity
					) ||
					!readBool(
						stream,
						object.
						rigidBodyIsStatic
					)
					)
				{
					return false;
				}

				if (storedVersion >= 3)
				{
					if (
						!readVec3(
							stream,
							object.
							rigidBodyColliderSize
						) ||
						!readVec3(
							stream,
							object.
							rigidBodyColliderOffset
						) ||
						!readBool(
							stream,
							object.
							rigidBodyColliderUsesTransformScale
						)
						)
					{
						return false;
					}
				}
			}
		}

		if (storedVersion >= 4)
		{
			if (!readBool(
				stream,
				object.hasMaterial
			))
			{
				return false;
			}

			if (object.hasMaterial)
			{
				if (
					!readString(
						stream,
						object.
						materialTexturePath
					) ||
					!readVec2(
						stream,
						object.
						materialUvTiling
					) ||
					!readVec2(
						stream,
						object.
						materialUvOffset
					)
					)
				{
					return false;
				}

				if (storedVersion >= 5)
				{
					if (!readVec4(
						stream,
						object.
						materialColor
					))
					{
						return false;
					}
				}
			}
		}

		switch (object.type)
		{
		case SceneObjectType::Cube:
		case SceneObjectType::Plane:
		case SceneObjectType::Sphere:
		case SceneObjectType::Capsule:
			return true;

		case SceneObjectType::CombinedMesh:
			return readMeshData(
				stream,
				object.meshData,
				storedVersion
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

	void instantiateObject(
		dx3d::World& world,
		const SerializedSceneObject& data,
		dx3d::SceneLoadResult& result
	)
	{
		auto* object =
			world.createGameObject<
			dx3d::GameObject
			>();

		if (!object)
		{
			return;
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

		case SceneObjectType::Sphere:
			object->createOrGetComponent<
				dx3d::SphereComponent
			>();

			++result.sphereCount;
			break;

		case SceneObjectType::Capsule:
			object->createOrGetComponent<
				dx3d::CapsuleComponent
			>();

			++result.capsuleCount;
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

		if (data.hasRigidBody)
		{
			auto* rigidBody =
				object->createOrGetComponent<
				dx3d::RigidBodyComponent
				>();

			rigidBody->setVelocity(
				data.rigidBodyVelocity
			);

			rigidBody->setAngularVelocity(
				data.
				rigidBodyAngularVelocity
			);

			rigidBody->setMass(
				data.rigidBodyMass
			);

			rigidBody->setRestitution(
				data.
				rigidBodyRestitution
			);

			rigidBody->setFriction(
				data.rigidBodyFriction
			);

			rigidBody->setUseGravity(
				data.
				rigidBodyUseGravity
			);

			rigidBody->setStatic(
				data.rigidBodyIsStatic
			);

			rigidBody->setColliderSize(
				data.
				rigidBodyColliderSize
			);

			rigidBody->setColliderOffset(
				data.
				rigidBodyColliderOffset
			);

			rigidBody->
				setColliderUsesTransformScale(
					data.
					rigidBodyColliderUsesTransformScale
				);
		}

		if (data.hasMaterial)
		{
			auto* material =
				object->createOrGetComponent<
				dx3d::MaterialComponent
				>();

			material->setTexturePath(
				data.materialTexturePath
			);

			material->setUvTiling(
				data.materialUvTiling
			);

			material->setUvOffset(
				data.materialUvOffset
			);

			material->setColor(
				data.materialColor
			);
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
	}

	bool writeLevelObject(
		std::ofstream& stream,
		dx3d::GameObject* object,
		SceneObjectType type
	)
	{
		if (!object ||
			!isLevelObjectType(type))
		{
			return false;
		}

		stream << "    {\n";

		stream << "      \"name\": ";
		writeJsonString(
			stream,
			object->getName()
		);
		stream << ",\n";

		stream << "      \"type\": ";
		writeJsonString(
			stream,
			getLevelTypeName(type)
		);
		stream << ",\n";

		auto& transform =
			object->getTransform();

		stream << "      \"transform\": {\n";
		stream << "        \"position\": ";
		writeLevelVec3(
			stream,
			transform.getPosition()
		);
		stream << ",\n";
		stream << "        \"rotation\": ";
		writeLevelVec3(
			stream,
			transform.getRotation()
		);
		stream << ",\n";
		stream << "        \"scale\": ";
		writeLevelVec3(
			stream,
			transform.getScale()
		);
		stream << "\n";
		stream << "      },\n";

		if (type ==
			SceneObjectType::DirectionalLight)
		{
			auto* light =
				object->getComponent<
				dx3d::DirectionalLightComponent
				>();

			if (!light)
			{
				return false;
			}

			stream << "      \"light\": {\n";
			stream << "        \"color\": ";
			writeLevelVec3(
				stream,
				light->getColor()
			);
			stream << ",\n";
			stream << "        \"intensity\": "
				<< light->getIntensity()
				<< ",\n";
			stream << "        \"ambientStrength\": "
				<< light->getAmbientStrength()
				<< ",\n";
			stream << "        \"shadowArea\": "
				<< light->getShadowArea()
				<< ",\n";
			stream << "        \"castShadows\": "
				<< light->getCastShadows()
				<< "\n";
			stream << "      },\n";
		}

		if (type !=
			SceneObjectType::DirectionalLight)
		{
			auto* material =
				object->getComponent<
				dx3d::MaterialComponent
				>();

			stream << "      \"material\": {\n";
			stream << "        \"texture\": ";
			writeJsonString(
				stream,
				material
				? material->getTexturePath()
				: std::string{}
			);
			stream << ",\n";
			stream << "        \"uvTiling\": ";
			writeLevelVec2(
				stream,
				material
				? material->getUvTiling()
				: dx3d::Vec2{ 1.0f, 1.0f }
			);
			stream << ",\n";
			stream << "        \"uvOffset\": ";
			writeLevelVec2(
				stream,
				material
				? material->getUvOffset()
				: dx3d::Vec2{}
			);
			stream << ",\n";
			stream << "        \"color\": ";
			writeLevelVec4(
				stream,
				material
				? material->getColor()
				: dx3d::Vec4{
					1.0f,
					1.0f,
					1.0f,
					1.0f
				}
			);
			stream << "\n";
			stream << "      },\n";
		}

		auto* rigidBody =
			object->getComponent<
			dx3d::RigidBodyComponent
			>();

		stream << "      \"rigidBody\": ";

		if (!rigidBody)
		{
			stream << "{ \"enabled\": false }\n";
		}
		else
		{
			stream << "{\n";
			stream << "        \"enabled\": true,\n";
			stream << "        \"velocity\": ";
			writeLevelVec3(
				stream,
				rigidBody->getVelocity()
			);
			stream << ",\n";
			stream << "        \"angularVelocity\": ";
			writeLevelVec3(
				stream,
				rigidBody->
				getAngularVelocity()
			);
			stream << ",\n";
			stream << "        \"mass\": "
				<< rigidBody->getMass()
				<< ",\n";
			stream << "        \"restitution\": "
				<< rigidBody->getRestitution()
				<< ",\n";
			stream << "        \"friction\": "
				<< rigidBody->getFriction()
				<< ",\n";
			stream << "        \"useGravity\": "
				<< rigidBody->getUseGravity()
				<< ",\n";
			stream << "        \"isStatic\": "
				<< rigidBody->getStatic()
				<< ",\n";
			stream << "        \"collider\": {\n";
			stream << "          \"shape\": \"box\",\n";
			stream << "          \"size\": ";
			writeLevelVec3(
				stream,
				rigidBody->getColliderSize()
			);
			stream << ",\n";
			stream << "          \"offset\": ";
			writeLevelVec3(
				stream,
				rigidBody->getColliderOffset()
			);
			stream << ",\n";
			stream << "          \"matchRenderScale\": "
				<< rigidBody->
				getColliderUsesTransformScale()
				<< "\n";
			stream << "        }\n";
			stream << "      }\n";
		}

		stream << "    }";

		return static_cast<bool>(stream);
	}

	bool readLevelObject(
		const JsonValue& value,
		SerializedSceneObject& object
	)
	{
		if (value.type !=
			JsonValue::Type::Object)
		{
			return false;
		}

		std::string typeName{};

		if (!readStringMember(
			value,
			"type",
			typeName
		))
		{
			return false;
		}

		if (!getLevelTypeFromName(
			typeName,
			object.type
		))
		{
			return false;
		}

		if (!isLevelObjectType(
			object.type
		))
		{
			return false;
		}

		if (!readStringMember(
			value,
			"name",
			object.name
		))
		{
			object.name =
				getLevelTypeName(
					object.type
				);
		}

		if (const auto* transform =
			findMember(
				value,
				"transform"
			);
			transform &&
			transform->type ==
			JsonValue::Type::Object)
		{
			readVec3Member(
				*transform,
				"position",
				object.position
			);

			readVec3Member(
				*transform,
				"rotation",
				object.rotation
			);

			readVec3Member(
				*transform,
				"scale",
				object.scale
			);
		}

		if (const auto* light =
			findMember(
				value,
				"light"
			);
			light &&
			light->type ==
			JsonValue::Type::Object)
		{
			readVec3Member(
				*light,
				"color",
				object.lightColor
			);

			readNumberMember(
				*light,
				"intensity",
				object.lightIntensity
			);

			readNumberMember(
				*light,
				"ambientStrength",
				object.ambientStrength
			);

			readNumberMember(
				*light,
				"shadowArea",
				object.shadowArea
			);

			readBoolMember(
				*light,
				"castShadows",
				object.castShadows
			);
		}

		if (const auto* material =
			findMember(
				value,
				"material"
			);
			material &&
			material->type ==
			JsonValue::Type::Object)
		{
			object.hasMaterial = true;

			readStringMember(
				*material,
				"texture",
				object.materialTexturePath
			);

			readVec2Member(
				*material,
				"uvTiling",
				object.materialUvTiling
			);

			readVec2Member(
				*material,
				"uvOffset",
				object.materialUvOffset
			);

			readVec4Member(
				*material,
				"color",
				object.materialColor
			);
		}

		if (const auto* rigidBody =
			findMember(
				value,
				"rigidBody"
			);
			rigidBody &&
			rigidBody->type ==
			JsonValue::Type::Object)
		{
			object.hasRigidBody = true;

			readBoolMember(
				*rigidBody,
				"enabled",
				object.hasRigidBody
			);

			if (object.hasRigidBody)
			{
				readVec3Member(
					*rigidBody,
					"velocity",
					object.
					rigidBodyVelocity
				);

				readVec3Member(
					*rigidBody,
					"angularVelocity",
					object.
					rigidBodyAngularVelocity
				);

				readNumberMember(
					*rigidBody,
					"mass",
					object.rigidBodyMass
				);

				readNumberMember(
					*rigidBody,
					"restitution",
					object.
					rigidBodyRestitution
				);

				readNumberMember(
					*rigidBody,
					"friction",
					object.
					rigidBodyFriction
				);

				readBoolMember(
					*rigidBody,
					"useGravity",
					object.
					rigidBodyUseGravity
				);

				readBoolMember(
					*rigidBody,
					"isStatic",
					object.rigidBodyIsStatic
				);

				if (const auto* collider =
					findMember(
						*rigidBody,
						"collider"
					);
					collider &&
					collider->type ==
					JsonValue::Type::Object)
				{
					readVec3Member(
						*collider,
						"size",
						object.
						rigidBodyColliderSize
					);

					readVec3Member(
						*collider,
						"offset",
						object.
						rigidBodyColliderOffset
					);

					readBoolMember(
						*collider,
						"matchRenderScale",
						object.
						rigidBodyColliderUsesTransformScale
					);
				}
			}
		}

		return true;
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
			storedVersion <
			minimumSupportedSceneVersion ||
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

		for (const auto& object :
			serializedObjects)
		{
			instantiateObject(
				world,
				object,
				result
			);
		}

		result.success = true;

		return result;
}

bool dx3d::SceneSerializer::saveLevel(
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

		if (
			getObjectType(
				object,
				type
			) &&
			isLevelObjectType(type)
			)
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
		maximumLevelObjectCount
		)
	{
		return false;
	}

	std::ofstream stream(
		filePath,
		std::ios::out |
		std::ios::trunc
	);

	if (!stream)
	{
		return false;
	}

	stream
		<< std::boolalpha
		<< std::setprecision(9);

	stream << "{\n";
	stream << "  \"format\": \"DX3D_LEVEL\",\n";
	stream << "  \"version\": 1,\n";
	stream << "  \"objects\": [\n";

	for (size_t index = 0;
		index < objectsToSave.size();
		++index)
	{
		if (!writeLevelObject(
			stream,
			objectsToSave[index].first,
			objectsToSave[index].second
		))
		{
			return false;
		}

		if (index + 1 <
			objectsToSave.size())
		{
			stream << ",";
		}

		stream << "\n";
	}

	stream << "  ]\n";
	stream << "}\n";

	return static_cast<bool>(stream);
}

dx3d::SceneLoadResult
dx3d::SceneSerializer::loadLevel(
	World& world,
	const std::string& filePath
)
{
	SceneLoadResult result{};

	std::ifstream stream(
		filePath
	);

	if (!stream)
	{
		return result;
	}

	const std::string fileData
	{
		std::istreambuf_iterator<char>(
			stream
		),
		std::istreambuf_iterator<char>()
	};

	JsonValue root{};
	JsonParser parser(fileData);

	if (!parser.parse(root))
	{
		return result;
	}

	const JsonValue* objectsValue =
		findMember(
			root,
			"objects"
		);

	if (
		!objectsValue ||
		objectsValue->type !=
		JsonValue::Type::Array ||
		objectsValue->arrayValues.size() >
		maximumLevelObjectCount
		)
	{
		return result;
	}

	std::vector<
		SerializedSceneObject
	> serializedObjects{};

	serializedObjects.reserve(
		objectsValue->arrayValues.size()
	);

	for (const auto& objectValue :
		objectsValue->arrayValues)
	{
		SerializedSceneObject object{};

		if (readLevelObject(
			objectValue,
			object
		))
		{
			serializedObjects.push_back(
				std::move(object)
			);
		}
	}

	clear(world);

	for (const auto& object :
		serializedObjects)
	{
		instantiateObject(
			world,
			object,
			result
		);
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
