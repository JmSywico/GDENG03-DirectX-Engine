#pragma once

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace jnpf::Core
{
	struct JobSystemStats
	{
		std::size_t WorkerCount = 0;
		std::size_t QueuedJobs = 0;
		std::size_t ActiveJobs = 0;
		std::size_t PeakQueuedJobs = 0;
		std::uint64_t CompletedJobs = 0;
		double AverageExecutionMs = 0.0;
	};

	/** Fixed worker pool for CPU-only work; main-thread commits remain explicit. */
	class JobSystem
	{
	public:
		explicit JobSystem(std::size_t workerCount = 0);
		~JobSystem();

		JobSystem(const JobSystem&) = delete;
		JobSystem& operator=(const JobSystem&) = delete;

		template <typename Function>
		auto Submit(Function&& function)
			-> std::future<std::invoke_result_t<std::decay_t<Function>>>
		{
			using Result = std::invoke_result_t<std::decay_t<Function>>;
			auto task = std::make_shared<std::packaged_task<Result()>>(
				std::forward<Function>(function));
			auto future = task->get_future();
			Enqueue([task] { (*task)(); });
			return future;
		}

		template <typename Function>
		auto SubmitCancelable(Function&& function)
			-> std::future<std::invoke_result_t<std::decay_t<Function>, std::stop_token>>
		{
			using Result = std::invoke_result_t<std::decay_t<Function>, std::stop_token>;
			const std::stop_token token = m_cancellation.get_token();
			auto task = std::make_shared<std::packaged_task<Result()>>(
				[callable = std::forward<Function>(function), token]() mutable
				{
					return callable(token);
				});
			auto future = task->get_future();
			Enqueue([task] { (*task)(); });
			return future;
		}

		template <typename Function>
		void ParallelFor(std::size_t count, std::size_t minimumGrain, Function&& function)
		{
			if (count == 0) return;
			const std::size_t workers = GetWorkerCount();
			if (workers == 0 || count <= minimumGrain)
			{
				for (std::size_t index = 0; index < count; ++index) function(index);
				return;
			}
			const std::size_t targetTasks = workers * 4;
			const std::size_t grain = std::max(
				minimumGrain,
				(count + targetTasks - 1) / targetTasks);
			auto sharedFunction = std::make_shared<std::decay_t<Function>>(
				std::forward<Function>(function));
			std::vector<std::future<void>> futures;
			for (std::size_t begin = 0; begin < count; begin += grain)
			{
				const std::size_t end = std::min(count, begin + grain);
				futures.push_back(Submit([sharedFunction, begin, end]
				{
					for (std::size_t index = begin; index < end; ++index)
						(*sharedFunction)(index);
				}));
			}
			for (auto& future : futures) future.get();
		}

		void PostMainThread(std::function<void()> completion);
		std::size_t DrainMainThread(std::size_t maximum = static_cast<std::size_t>(-1));
		void WaitIdle();
		void CancelPending();
		void ResetCancellation();
		std::size_t GetWorkerCount() const { return m_workers.size(); }
		JobSystemStats GetStats() const;

	private:
		void Enqueue(std::function<void()> job);
		void WorkerLoop(std::stop_token stopToken);

		std::vector<std::jthread> m_workers;
		mutable std::mutex m_queueMutex;
		std::condition_variable_any m_queueCondition;
		std::deque<std::function<void()>> m_jobs;
		std::mutex m_completionMutex;
		std::deque<std::function<void()>> m_mainThreadCompletions;
		std::atomic_bool m_stopping = false;
		std::atomic_size_t m_activeJobs = 0;
		std::atomic_size_t m_peakQueuedJobs = 0;
		std::atomic_uint64_t m_completedJobs = 0;
		std::atomic_uint64_t m_totalExecutionNanoseconds = 0;
		std::stop_source m_cancellation;
	};
}
