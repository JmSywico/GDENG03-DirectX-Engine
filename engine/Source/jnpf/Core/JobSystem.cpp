#include "Core/JobSystem.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace jnpf::Core
{
	JobSystem::JobSystem(std::size_t workerCount)
	{
		if (workerCount == 0)
		{
			const unsigned int hardware = std::thread::hardware_concurrency();
			workerCount = hardware > 1 ? static_cast<std::size_t>(hardware - 1) : 1;
			workerCount = std::min<std::size_t>(workerCount, 12);
		}
		m_workers.reserve(workerCount);
		for (std::size_t index = 0; index < workerCount; ++index)
			m_workers.emplace_back([this](std::stop_token stopToken) { WorkerLoop(stopToken); });
	}

	JobSystem::~JobSystem()
	{
		m_cancellation.request_stop();
		m_stopping.store(true, std::memory_order_release);
		for (auto& worker : m_workers) worker.request_stop();
		m_queueCondition.notify_all();
		m_workers.clear();
	}

	void JobSystem::Enqueue(std::function<void()> job)
	{
		if (!job) return;
		{
			std::scoped_lock lock(m_queueMutex);
			if (m_stopping.load(std::memory_order_acquire))
				throw std::runtime_error("job system is stopping");
			m_jobs.push_back(std::move(job));
			const std::size_t queued = m_jobs.size();
			std::size_t peak = m_peakQueuedJobs.load(std::memory_order_relaxed);
			while (peak < queued && !m_peakQueuedJobs.compare_exchange_weak(peak, queued)) {}
		}
		m_queueCondition.notify_one();
	}

	void JobSystem::WorkerLoop(std::stop_token stopToken)
	{
		while (true)
		{
			std::function<void()> job;
			{
				std::unique_lock lock(m_queueMutex);
				m_queueCondition.wait(lock, stopToken, [this]
				{
					return m_stopping.load(std::memory_order_acquire) || !m_jobs.empty();
				});
				if (m_jobs.empty())
				{
					if (stopToken.stop_requested() || m_stopping.load(std::memory_order_acquire)) return;
					continue;
				}
				job = std::move(m_jobs.front());
				m_jobs.pop_front();
				m_activeJobs.fetch_add(1, std::memory_order_relaxed);
			}
			const auto started = std::chrono::steady_clock::now();
			job();
			const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
				std::chrono::steady_clock::now() - started).count();
			m_totalExecutionNanoseconds.fetch_add(
				static_cast<std::uint64_t>(std::max<std::int64_t>(0, elapsed)),
				std::memory_order_relaxed);
			m_completedJobs.fetch_add(1, std::memory_order_relaxed);
			m_activeJobs.fetch_sub(1, std::memory_order_relaxed);
			m_queueCondition.notify_all();
		}
	}

	void JobSystem::PostMainThread(std::function<void()> completion)
	{
		if (!completion) return;
		std::scoped_lock lock(m_completionMutex);
		m_mainThreadCompletions.push_back(std::move(completion));
	}

	std::size_t JobSystem::DrainMainThread(std::size_t maximum)
	{
		std::size_t drained = 0;
		while (drained < maximum)
		{
			std::function<void()> completion;
			{
				std::scoped_lock lock(m_completionMutex);
				if (m_mainThreadCompletions.empty()) break;
				completion = std::move(m_mainThreadCompletions.front());
				m_mainThreadCompletions.pop_front();
			}
			completion();
			++drained;
		}
		return drained;
	}

	void JobSystem::WaitIdle()
	{
		std::unique_lock lock(m_queueMutex);
		m_queueCondition.wait(lock, [this]
		{
			return m_jobs.empty() && m_activeJobs.load(std::memory_order_acquire) == 0;
		});
	}

	void JobSystem::CancelPending()
	{
		m_cancellation.request_stop();
		{
			std::scoped_lock lock(m_queueMutex);
			m_jobs.clear();
		}
		m_queueCondition.notify_all();
	}

	void JobSystem::ResetCancellation()
	{
		WaitIdle();
		m_cancellation = std::stop_source{};
	}

	JobSystemStats JobSystem::GetStats() const
	{
		JobSystemStats stats;
		stats.WorkerCount = m_workers.size();
		{
			std::scoped_lock lock(m_queueMutex);
			stats.QueuedJobs = m_jobs.size();
		}
		stats.ActiveJobs = m_activeJobs.load(std::memory_order_relaxed);
		stats.PeakQueuedJobs = m_peakQueuedJobs.load(std::memory_order_relaxed);
		stats.CompletedJobs = m_completedJobs.load(std::memory_order_relaxed);
		if (stats.CompletedJobs != 0)
			stats.AverageExecutionMs = static_cast<double>(
				m_totalExecutionNanoseconds.load(std::memory_order_relaxed))
				/ static_cast<double>(stats.CompletedJobs) / 1'000'000.0;
		return stats;
	}
}
