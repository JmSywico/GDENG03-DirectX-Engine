#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class InputManager;

namespace jnpf::Input
{
	enum class BindingType : std::uint8_t { Key, MouseButton, GamepadButton, GamepadAxis };
	enum class GamepadAxis : std::uint16_t { LeftX, LeftY, RightX, RightY, LeftTrigger, RightTrigger };

	struct ActionBinding
	{
		BindingType Type = BindingType::Key;
		std::uint16_t Code = 0;
		float Scale = 1.0f;
		std::uint8_t Gamepad = 0;
	};

	class InputActionMap
	{
	public:
		using ValueProvider = std::function<float(const ActionBinding&)>;
		void Bind(std::string action, ActionBinding binding);
		void Clear();
		void Update(const InputManager& input);
		void UpdateFromValues(const ValueProvider& provider);
		float GetValue(const std::string& action) const;
		bool IsDown(const std::string& action) const;
		bool WasPressed(const std::string& action) const;
		bool WasReleased(const std::string& action) const;
		bool Save(const std::filesystem::path& path) const;
		bool Load(const std::filesystem::path& path);
		static InputActionMap CreateDefaults();

	private:
		std::unordered_map<std::string, std::vector<ActionBinding>> m_bindings;
		std::unordered_map<std::string, float> m_values;
		std::unordered_map<std::string, float> m_previousValues;
	};
}
