#include "Input/InputActions.h"

#include "Input/Input.h"

#include <Xinput.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>

namespace enignE::Input
{
	void InputActionMap::Bind(std::string action, ActionBinding binding)
	{
		m_bindings[std::move(action)].push_back(binding);
	}

	void InputActionMap::Clear()
	{
		m_bindings.clear(); m_values.clear(); m_previousValues.clear();
	}

	void InputActionMap::Update(const InputManager& input)
	{
		XINPUT_STATE gamepads[XUSER_MAX_COUNT]{};
		bool connected[XUSER_MAX_COUNT]{};
		for (DWORD index = 0; index < XUSER_MAX_COUNT; ++index)
			connected[index] = XInputGetState(index, &gamepads[index]) == ERROR_SUCCESS;
		UpdateFromValues([&](const ActionBinding& binding)
		{
			switch (binding.Type)
			{
			case BindingType::Key:
				return input.IsKeyDown(binding.Code) ? binding.Scale : 0.0f;
			case BindingType::MouseButton:
				return input.IsMouseButtonDown(binding.Code) ? binding.Scale : 0.0f;
			case BindingType::GamepadButton:
				return binding.Gamepad < XUSER_MAX_COUNT && connected[binding.Gamepad]
					&& (gamepads[binding.Gamepad].Gamepad.wButtons & binding.Code)
					? binding.Scale : 0.0f;
			case BindingType::GamepadAxis:
				break;
			}
			if (binding.Gamepad >= XUSER_MAX_COUNT || !connected[binding.Gamepad]) return 0.0f;
			const XINPUT_GAMEPAD& pad = gamepads[binding.Gamepad].Gamepad;
			float value = 0.0f;
			switch (static_cast<GamepadAxis>(binding.Code))
			{
			case GamepadAxis::LeftX: value = pad.sThumbLX / 32767.0f; break;
			case GamepadAxis::LeftY: value = pad.sThumbLY / 32767.0f; break;
			case GamepadAxis::RightX: value = pad.sThumbRX / 32767.0f; break;
			case GamepadAxis::RightY: value = pad.sThumbRY / 32767.0f; break;
			case GamepadAxis::LeftTrigger: value = pad.bLeftTrigger / 255.0f; break;
			case GamepadAxis::RightTrigger: value = pad.bRightTrigger / 255.0f; break;
			}
			if (std::abs(value) < 0.15f) value = 0.0f;
			return std::clamp(value * binding.Scale, -1.0f, 1.0f);
		});
	}

	void InputActionMap::UpdateFromValues(const ValueProvider& provider)
	{
		m_previousValues = m_values;
		for (const auto& [action, bindings] : m_bindings)
		{
			float value = 0.0f;
			for (const ActionBinding& binding : bindings)
				value += provider ? provider(binding) : 0.0f;
			m_values[action] = std::clamp(value, -1.0f, 1.0f);
		}
	}

	float InputActionMap::GetValue(const std::string& action) const
	{
		const auto found = m_values.find(action); return found == m_values.end() ? 0.0f : found->second;
	}
	bool InputActionMap::IsDown(const std::string& action) const { return std::abs(GetValue(action)) > 0.5f; }
	bool InputActionMap::WasPressed(const std::string& action) const
	{
		const auto previous = m_previousValues.find(action);
		return IsDown(action) && (previous == m_previousValues.end() || std::abs(previous->second) <= 0.5f);
	}
	bool InputActionMap::WasReleased(const std::string& action) const
	{
		const auto previous = m_previousValues.find(action);
		return !IsDown(action) && previous != m_previousValues.end() && std::abs(previous->second) > 0.5f;
	}

	bool InputActionMap::Save(const std::filesystem::path& path) const
	{
		nlohmann::json root{{"version", 1}, {"actions", nlohmann::json::object()}};
		for (const auto& [action, bindings] : m_bindings)
			for (const ActionBinding& binding : bindings)
				root["actions"][action].push_back({{"type", static_cast<int>(binding.Type)},
					{"code", binding.Code}, {"scale", binding.Scale}, {"gamepad", binding.Gamepad}});
		std::error_code error;
		if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), error);
		std::ofstream output(path, std::ios::binary | std::ios::trunc);
		return output && static_cast<bool>(output << root.dump(2));
	}

	bool InputActionMap::Load(const std::filesystem::path& path)
	{
		try
		{
			std::ifstream input(path, std::ios::binary);
			const nlohmann::json root = nlohmann::json::parse(input);
			if (root.at("version") != 1 || !root.at("actions").is_object()) return false;
			InputActionMap loaded;
			for (const auto& [action, items] : root["actions"].items())
				for (const auto& item : items)
					loaded.Bind(action, {static_cast<BindingType>(item.at("type").get<int>()),
						item.at("code").get<std::uint16_t>(), item.value("scale", 1.0f),
						item.value("gamepad", std::uint8_t{0})});
			*this = std::move(loaded); return true;
		}
		catch (...) { return false; }
	}

	InputActionMap InputActionMap::CreateDefaults()
	{
		InputActionMap map;
		map.Bind("MoveForward", {BindingType::Key, 'W', 1.0f});
		map.Bind("MoveForward", {BindingType::Key, 'S', -1.0f});
		map.Bind("MoveForward", {BindingType::GamepadAxis, static_cast<std::uint16_t>(GamepadAxis::LeftY), 1.0f});
		map.Bind("MoveRight", {BindingType::Key, 'D', 1.0f});
		map.Bind("MoveRight", {BindingType::Key, 'A', -1.0f});
		map.Bind("MoveRight", {BindingType::GamepadAxis, static_cast<std::uint16_t>(GamepadAxis::LeftX), 1.0f});
		map.Bind("MoveUp", {BindingType::Key, 'E', 1.0f});
		map.Bind("MoveUp", {BindingType::Key, 'Q', -1.0f});
		map.Bind("Boost", {BindingType::Key, VK_SHIFT, 1.0f});
		map.Bind("Jump", {BindingType::Key, VK_SPACE, 1.0f});
		map.Bind("Jump", {BindingType::GamepadButton, XINPUT_GAMEPAD_A, 1.0f});
		map.Bind("Primary", {BindingType::MouseButton, 1, 1.0f});
		map.Bind("Primary", {BindingType::GamepadButton, XINPUT_GAMEPAD_RIGHT_SHOULDER, 1.0f});
		return map;
	}
}
