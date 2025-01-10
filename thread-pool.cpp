#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class thread_pool_t final
{
  public:
    explicit thread_pool_t(const uint32_t num_threads = std::thread::hardware_concurrency())
    {
        std::cout << "Num threads :: " << num_threads << std::endl;

        m_threads.reserve(num_threads);

        // TODO: Launch n threads that all run a predefined function.
        for (uint32_t i = 0; i < num_threads; i++)
        {
            m_threads.emplace_back([this]() {
                while (!m_stop_processing_tasks)
                {
                    std::function<void()> current_task = {};

                    // Assumption is that if this thread is already running a function, the control flow will read this
                    // particular part ONLY if the function / task its execution is completed.
                    std::unique_lock<std::mutex> lock(m_queue_mutex);

                    m_cv.wait(lock, [this] { return !m_task_queue.empty() || m_stop_processing_tasks; });
                    if (m_stop_processing_tasks)
                    {
                        return;
                    }

                    // Take the top most task from the queue and start executing it.
                    current_task = std::move(m_task_queue.front());
                    m_task_queue.pop();

                    m_cv.notify_all();
                    current_task();
                }
            });
        }
    }

    ~thread_pool_t()
    {
        m_stop_processing_tasks = true;
        m_cv.notify_all();

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
    // NOTE: F and Args are && (r-values), so that perfect forwarding can work as expected.
    // F is the function, while args is a VAL.
    // perfect forwarding is used so that the 'type' of arg (i.e l / r value) is preserved.
    // decltype is used to deduce at compile type return type of func(args...)
    // Because the thread pools task queue only accepts functions that take and return NO parameters, bind is used to
    // 'bind' the function and arguments to create a callbale func that is 0 args.
    // A 'packaged task' is used to store the callable and return a future, which the host can use to get value of
    // function.
    template <typename F, typename... Args>
    auto enqueue(F &&func, Args &&...args) -> std::future<decltype(func(args...))>
    {
        // Bind the function and args to a single variable.
        std::function<decltype(func(args...))()> binded_function =
            std::bind(std::forward<F>(func), std::forward<Args>(args)...);

        // Shared pointer to package task is created so that lifetime is properly managed.
        auto shared_ptr_of_packaged_task =
            std::make_shared<std::packaged_task<decltype(func(args...))()>>(binded_function);

        std::future<decltype(func(args...))> future = shared_ptr_of_packaged_task->get_future();

        std::unique_lock<std::mutex> lock(m_queue_mutex);

        // add enqueued tasks to a queue.
        // The lambda capture shared_ptr_of_packaged_task by value so that lifetime of the package task will be until
        // the function is not finished execution.
        m_task_queue.push([shared_ptr_of_packaged_task]() { (*shared_ptr_of_packaged_task)(); });

        m_cv.notify_one();

        return future;
    }

  private:
    std::vector<std::thread> m_threads{};
    std::queue<std::function<void()>> m_task_queue{};
    std::mutex m_queue_mutex{};
    std::condition_variable m_cv{};
    std::atomic<bool> m_stop_processing_tasks{false};
};

int main()
{
    thread_pool_t thread_pool{};

    for (int i = 0; i < 100; i++)
    {
        thread_pool.enqueue([]() {
            std::cout << "Work started by thread : " << std::this_thread::get_id() << '\n';
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            std::cout << "Work completed by thread : " << std::this_thread::get_id() << '\n';
        });
    }

    auto computation_result = thread_pool.enqueue([]() { return 33 * 55 * 99 * 33; });

    while (computation_result.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
    {
    }

    std::cout << "Computation result :: " << computation_result.get();
}
