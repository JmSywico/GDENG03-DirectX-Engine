#pragma once

#include <cstdint>

enum class EventType : std::uint32_t
{
	None = 0,

	WindowResized,
	WindowClosed,

	KeyDown,
	KeyUp,
	MouseMove,
	MouseButtonDown,
	MouseButtonUp,
	MouseWheel,

	FrameBegin,
	FrameEnd,

	COUNT
};

struct Event
{
	explicit Event(EventType type) : Type(type)
	{
	}

	virtual ~Event() = default;

	EventType Type;
	bool Handled = false;
};

struct FrameBeginEvent : Event
{
	float DeltaTime;

	FrameBeginEvent(float dt = 0.0f)
	: Event(EventType::FrameBegin)
	, DeltaTime(dt)
	{
	}
};

struct FrameEndEvent : Event
{
	float DeltaTime;

	FrameEndEvent(float dt = 0.0f)
	: Event(EventType::FrameEnd)
	, DeltaTime(dt)
	{
	}
};
