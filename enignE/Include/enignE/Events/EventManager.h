#pragma once

#include "../pch.h"
#include "Event.h"

class EventManager
{
public:
	using ListenerID = std::uint64_t;
	using EventCallback = std::function<void(Event&)>;

	static EventManager& Get();

	EventManager(const EventManager&) = delete;
	EventManager& operator=(const EventManager&) = delete;

	/** @return Listener ID that can be used to unregister the callback. */
	ListenerID AddListener(EventType type, EventCallback callback);

	void RemoveListener(EventType type, ListenerID id);

	/** @brief Queues an event for dispatch on the next event pump. */
	void QueueEvent(std::unique_ptr<Event> evt);

	/** @brief Dispatches immediately on the caller's thread. */
	void FireEvent(Event& evt);

	void DispatchQueuedEvents();

private:
	EventManager();
	~EventManager() = default;

	struct ListenerEntry
	{
		ListenerID Id;
		EventCallback Callback;
	};

	std::mutex m_queueMutex;
	std::queue<std::unique_ptr<Event>> m_eventQueue;

	std::mutex m_listenersMutex;
	std::unordered_map<EventType, std::vector<ListenerEntry>> m_listeners;

	ListenerID m_nextListenerId;
};
