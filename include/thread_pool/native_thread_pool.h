#ifndef MAIL_SYSTEM_NATIVE_THREAD_POOL_H
#define MAIL_SYSTEM_NATIVE_THREAD_POOL_H

// 纯标准库线程池，替换原先基于 boost::asio::thread_pool 的实现。
//
// 原 BoostThreadPool 的能力面只有 post(闭包) —— 没有 timer、没有 strand、
// 没有 executor —— 用 asio 承载它属于杀鸡用牛刀，代价是整个项目背上 Boost 依赖。
//
// 语义与 BoostThreadPool 的两点差异（有意为之）：
//   1. 未启动/已停止时 post() 只记录并丢弃，不抛异常。原实现抛 std::runtime_error，
//      而从网络回调里抛异常会直接打断事件循环——服务化之后这个行为更危险。
//   2. stop() 默认先跑完队列（drain）再退线程，便于优雅停机时把在途战斗算完。

#include "thread_pool_base.h"

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

class NativeThreadPool : public ThreadPoolBase {
public:
    explicit NativeThreadPool(size_t thread_count = 0)
        : m_thread_count(thread_count == 0 ? 4u : thread_count), m_running(false) {}

    ~NativeThreadPool() override {
        stop(true);
    }

    void start() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_running.load()) {
            return;
        }
        m_stopping = false;
        m_running.store(true);
        m_threads.reserve(m_thread_count);
        for (size_t i = 0; i < m_thread_count; ++i) {
            m_threads.emplace_back([this] { worker_loop(); });
        }
    }

    // wait_for_tasks=false 时丢弃未执行任务立刻退线程（进程要退出时的快路径）。
    void stop(bool wait_for_tasks = true) override {
        std::vector<std::thread> threads;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_running.load()) {
                return;
            }
            if (!wait_for_tasks) {
                m_tasks.clear();
            }
            m_stopping = true;
            m_running.store(false);
            threads.swap(m_threads);
        }
        m_cv.notify_all();
        for (auto& t : threads) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

    size_t thread_count() const override {
        return m_thread_count;
    }

    bool is_running() const override {
        return m_running.load();
    }

    // 当前排队中（未开始执行）的任务数，压测/观测用。
    size_t pending() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_tasks.size();
    }

protected:
    template <class F, class... Args>
    auto submit_impl(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using return_type = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<return_type> result = task->get_future();

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_running.load()) {
                throw std::runtime_error("NativeThreadPool is not running");
            }
            m_tasks.emplace_back([task] { (*task)(); });
        }
        m_cv.notify_one();
        return result;
    }

    void post_impl(std::function<void()> f) override {
        if (!f) {
            return;
        }
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_running.load()) {
                std::fprintf(stderr, "[pool] post() ignored: pool not running\n");
                return;
            }
            m_tasks.push_back(std::move(f));
        }
        m_cv.notify_one();
    }

private:
    void worker_loop() {
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait(lock, [this] { return m_stopping || !m_tasks.empty(); });
                if (m_tasks.empty()) {
                    // 只有 stop() 会把 stopping 置真；drain 语义 = 队列真的空了才走。
                    if (m_stopping) {
                        return;
                    }
                    continue;
                }
                task = std::move(m_tasks.front());
                m_tasks.pop_front();
            }
            try {
                task();
            } catch (const std::exception& ex) {
                // 任务异常不得弄死 worker，否则线程池会静默缩容。
                std::fprintf(stderr, "[pool] task threw: %s\n", ex.what());
            } catch (...) {
                std::fprintf(stderr, "[pool] task threw unknown exception\n");
            }
        }
    }

    size_t m_thread_count;
    std::deque<std::function<void()>> m_tasks;
    std::vector<std::thread> m_threads;
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::atomic<bool> m_running;
    bool m_stopping = false;
};

#endif  // MAIL_SYSTEM_NATIVE_THREAD_POOL_H
