#ifndef THREADPOOL_THREADPOOL_H
#define THREADPOOL_THREADPOOL_H

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <typeinfo>
#include <vector>

namespace KZX {
class ThreadPool {
 public:
  ThreadPool();

  ThreadPool(int min, int max, int timeout_thread, int timeout_watch);

  ThreadPool(const ThreadPool &) = delete;

  ThreadPool(ThreadPool &&) = delete;

  ~ThreadPool();

  ThreadPool &operator=(const ThreadPool &) = delete;

  ThreadPool &operator=(ThreadPool &&) = delete;

  template<class F, class... Args>
  void AddTask(F &&f, Args &&... args);

  template<class F, class... Args>
  auto AddTaskAndResult(F &&f,
                        Args &&... args) -> std::future<decltype(f(args...))>;

  bool get_stopped() { return stopped_; }

  int get_current_thread_size() { return current_thread_size_.load(); }

  void Wait();

 private:
  using Func = std::function<void()>;
  const int kMaxThreadSize;
  const int kMinThreadSize;

  void Init();

  void CreateThread();

  void WatchThread();

  void Stop();

  int thread_timeout_;
  int watch_timeout_;

  std::atomic<int> current_thread_size_;
  std::atomic<int> idle_thread_size_;
  int current_task_size_;
  bool stopped_;
  bool wait_status_;

  std::condition_variable cond_;
  std::mutex task_mutex_;
  std::mutex thread_mutex_;
  std::vector<std::thread> threads_;
  std::queue<Func> tasks_;
};

template<class F, class... Args>
void ThreadPool::AddTask(F &&f, Args &&... args) {
  if (stopped_ || wait_status_) {
    throw std::runtime_error("线程池结束后，再次向线程池加入任务");
  }

  auto task = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
  {
    std::lock_guard<std::mutex> task_lock(task_mutex_);
    current_task_size_++;
    tasks_.emplace(task);
  }

  cond_.notify_one();
}

template<class F, class... Args>
auto ThreadPool::AddTaskAndResult(F &&f,
                                  Args &&... args) -> std::future<decltype(f(
    args...))> {

  if (stopped_ || wait_status_) {
    throw std::runtime_error("线程池结束后，再次向线程池加入任务");
  }

  using return_type = decltype(f(args...));

  auto task = std::make_shared<std::packaged_task<return_type()>>(
      std::bind(std::forward<F>(f), std::forward<Args>(args)...)
  );

  std::future<return_type> res = task->get_future();
  {
    std::lock_guard<std::mutex> task_lock(task_mutex_);
    current_task_size_++;
    tasks_.emplace([task]() {
      (*task)();
    });
  }
  cond_.notify_one();
  return res;
}

}//namespace KZX
#endif //THREADPOOL_THREADPOOL_H
