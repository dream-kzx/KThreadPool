#include "ThreadPool.h"

namespace KZX {
ThreadPool::ThreadPool()
    : kMaxThreadSize(5),
      kMinThreadSize(2),
      thread_timeout_(1000),
      watch_timeout_(100),
      current_thread_size_(0),
      idle_thread_size_(0),
      current_task_size_(0),
      stopped_(false),
      wait_status_(false) {
  Init();
}

// min, 设置最小线程数
// max，设置最大线程数
// timeout_thread, 线程的超时时间，单位为毫秒
// timeout_watch，检测线程的定时时间，单位为毫秒
ThreadPool::ThreadPool(int min, int max, int thread_timeout, int watch_timeout)
    : kMaxThreadSize(max),
      kMinThreadSize(min),
      thread_timeout_(thread_timeout),
      watch_timeout_(watch_timeout),
      current_thread_size_(0),
      idle_thread_size_(0),
      current_task_size_(0),
      stopped_(false),
      wait_status_(false) {
  if (min > max) {
    stopped_ = true;
    return;
  }
  if (thread_timeout < 0) thread_timeout_ = 1000;
  if (watch_timeout < 0) watch_timeout_ = 100;

  Init();
};

ThreadPool::~ThreadPool() {
  if (!stopped_) {
    Stop();
  }
}

//等待线程池所有任务（包括任务队列）执行完毕
void ThreadPool::Wait() {
  wait_status_ = true;
  for (;;) {
    if (current_task_size_ == 0) {
      Stop();
    }
    if (current_task_size_ == 0 && current_thread_size_ == 0) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

//初始化函数，创建初始线程以及监听线程
void ThreadPool::Init() {
  for (int i = 0; i < kMinThreadSize; ++i) {
    std::lock_guard<std::mutex> thread_lock(thread_mutex_);
    if (current_thread_size_ < kMinThreadSize) {
      ++current_thread_size_;
      ++idle_thread_size_;

      threads_.emplace_back([this] { CreateThread(); });
    } else {
      break;
    }
  }

  std::lock_guard<std::mutex> thread_lock(thread_mutex_);
  threads_.emplace_back([this] { WatchThread(); });
}

//创建新的线程
void ThreadPool::CreateThread() {
  for (;;) {
    std::unique_lock<std::mutex> task_lock(task_mutex_);
    if (tasks_.empty()) {  //如果任务队列为空，则进去等待
      //在线程池停止，任务队列不为空或者超时的情况下，唤醒
      cond_.wait_for(task_lock,
                     std::chrono::milliseconds(thread_timeout_),
                     [this] { return stopped_ || !tasks_.empty(); });
      if (stopped_) {  //如果线程池关闭，则跳出循环
        break;
      } else if (!tasks_.empty()) {  //如果任务队列不为空，则进入下一次循环
        continue;
      } else {  //其他情况，即也就是超时情况
        if (current_thread_size_ > kMinThreadSize) {  //如果当前线程数大于最小线程数
          break;
        }
      }
    } else if (stopped_) {  //如果线程池关闭，则跳出循环
      break;
    } else if (!tasks_.empty()) {  //如果任务队列不为空，则获取任务执行
      auto task = std::move(tasks_.front());
      tasks_.pop();
      --idle_thread_size_;
      --current_task_size_;
      task_lock.unlock();
      task();
      task_lock.lock();
      ++idle_thread_size_;
    }
  }

  --current_thread_size_;
  --idle_thread_size_;
}

//检测线程，用于动态调度线程池的线程个数
void ThreadPool::WatchThread() {
  while (!stopped_) {
    std::lock_guard<std::mutex> thread_lock(thread_mutex_);
    if ((current_thread_size_.load() < kMinThreadSize) ||
        (current_task_size_ > idle_thread_size_.load() &&
            current_thread_size_.load() < kMaxThreadSize)) {
      ++current_thread_size_;
      ++idle_thread_size_;

      threads_.emplace_back([this] { CreateThread(); });
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(watch_timeout_));
  }
}

//停止线程池运行，唤醒所有睡眠进程
void ThreadPool::Stop() {
  stopped_ = true;
  cond_.notify_all();
  for (auto &th : threads_) {
    if (th.joinable()) th.join();
  }
}

}//namespace KZX
