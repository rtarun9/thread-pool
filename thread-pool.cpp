#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class thread_pool_t final
{
  public:
    explicit thread_pool_t(const uint32_t num_threads)
    {
        m_threads.reserve(num_threads);

        // TODO: Launch n threads that all run a predefined function.
        for (uint32_t i = 0; i < num_threads; i++)
        {
            m_threads.emplace_back(&thread_pool_t::thread_worker, this);
        }
    }

    ~thread_pool_t()
    {
        m_stop_processing_tasks = true;

        for (auto &thread : m_threads)
        {
            thread.join();
        }
    }

  private:
    thread_pool_t(const thread_pool_t &other) = delete;
    thread_pool_t &operator=(const thread_pool_t &other) = delete;

    thread_pool_t(thread_pool_t &&other) = delete;
    thread_pool_t &operator=(thread_pool_t &&other) = delete;

  public:
    template <typename F> void enqueue(F &&func)
    {
        std::scoped_lock<std::mutex> scoped_lock(m_queue_mutex);

        // add enqueued tasks to a queue.
        m_task_queue.push(func);
    }

  private:
    // Function that each thread calls endlessly.
    void thread_worker()
    {
        while (!m_stop_processing_tasks)
        {
            std::function<void()> current_task = {};

            // Assumption is that if this thread is already running a function, the control flow will read this
            // particular part ONLY if the function / task its execution is completed.
            std::scoped_lock<std::mutex> scoped_lock(m_queue_mutex);

            if (m_task_queue.empty() || m_stop_processing_tasks)
            {
                continue;
            }
            else
            {
                // Take the top most task from the queue and start executing it.
                current_task = m_task_queue.front();
                m_task_queue.pop();
            }

            current_task();
        }
    }

  private:
    std::vector<std::thread> m_threads{};
    std::queue<std::function<void()>> m_task_queue{};
    std::mutex m_queue_mutex{};
    bool m_stop_processing_tasks{false};
};

int main()
{
    thread_pool_t thread_pool{5};

    for (int i = 0; i < 100; i++)
    {
        thread_pool.enqueue([]() {
            std::cout << "Work started by thread : " << std::this_thread::get_id() << '\n';
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            std::cout << "Work completed by thread : " << std::this_thread::get_id() << '\n';
        });
    }
    std::this_thread::sleep_for(std::chrono::seconds(10));
}
