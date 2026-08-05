#include <DX3D/Input/InputActionMap.h>
#include <DX3D/Input/InputSystem.h>

#include <algorithm>
#include <cmath>
#include <utility>

void dx3d::InputActionMap::bind(std::string action, InputActionBinding binding)
{
	m_bindings[std::move(action)].push_back(binding);
}

void dx3d::InputActionMap::clear()
{
	m_bindings.clear();
	m_values.clear();
	m_previousValues.clear();
}

void dx3d::InputActionMap::update(const InputSystem& input)
{
	m_previousValues = m_values;
	for (const auto& [action, bindings] : m_bindings)
	{
		f32 value = 0.0f;
		for (const InputActionBinding& binding : bindings)
			if (input.isKeyDown(binding.key)) value += binding.scale;
		m_values[action] = std::clamp(value, -1.0f, 1.0f);
	}
}

dx3d::f32 dx3d::InputActionMap::getValue(const std::string& action) const
{
	const auto found = m_values.find(action);
	return found == m_values.end() ? 0.0f : found->second;
}

bool dx3d::InputActionMap::isDown(const std::string& action) const
{
	return std::abs(getValue(action)) > 0.5f;
}

bool dx3d::InputActionMap::wasPressed(const std::string& action) const
{
	const auto previous = m_previousValues.find(action);
	return isDown(action) &&
		(previous == m_previousValues.end() || std::abs(previous->second) <= 0.5f);
}

bool dx3d::InputActionMap::wasReleased(const std::string& action) const
{
	const auto previous = m_previousValues.find(action);
	return !isDown(action) && previous != m_previousValues.end() &&
		std::abs(previous->second) > 0.5f;
}

dx3d::InputActionMap dx3d::InputActionMap::createDefaults()
{
	InputActionMap map;
	map.bind("MoveForward", { KeyCode::W, 1.0f });
	map.bind("MoveForward", { KeyCode::S, -1.0f });
	map.bind("MoveRight", { KeyCode::D, 1.0f });
	map.bind("MoveRight", { KeyCode::A, -1.0f });
	map.bind("MoveUp", { KeyCode::E, 1.0f });
	map.bind("MoveUp", { KeyCode::Q, -1.0f });
	map.bind("Boost", { KeyCode::Shift, 1.0f });
	map.bind("Primary", { KeyCode::MouseLeft, 1.0f });
	return map;
}
