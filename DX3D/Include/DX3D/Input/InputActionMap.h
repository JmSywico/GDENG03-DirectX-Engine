#pragma once

#include <DX3D/Core/Common.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace dx3d
{
	class InputSystem;

	struct InputActionBinding
	{
		KeyCode key{};
		f32 scale{ 1.0f };
	};

	class InputActionMap final
	{
	public:
		void bind(std::string action, InputActionBinding binding);
		void clear();
		void update(const InputSystem& input);
		f32 getValue(const std::string& action) const;
		bool isDown(const std::string& action) const;
		bool wasPressed(const std::string& action) const;
		bool wasReleased(const std::string& action) const;
		static InputActionMap createDefaults();

	private:
		std::unordered_map<std::string, std::vector<InputActionBinding>> m_bindings{};
		std::unordered_map<std::string, f32> m_values{};
		std::unordered_map<std::string, f32> m_previousValues{};
	};
}
