export module makeDotCpp.thread:ThreadPool;
import std;

#include "alias.hpp"
#include "macro.hpp"

namespace makeDotCpp {
export class ThreadPool {
 public:
  DEF_EXCEPTION(IsTerminated, (const std::string& when),
                "thread pool is terminating when " + when);

  using RetType = std::any;
  using Func = RetType(ThreadPool&);
  using Task = std::packaged_task<Func>;

 private:
  CHAIN_VAR(std::size_t, threadNum, setSize);

 private:
  std::vector<std::thread> threadList;
  std::size_t freeThread = 0;
  std::queue<Task> taskQueue;
  bool isTerminated = false;
  std::mutex mutex;
  std::condition_variable workCv;
  std::condition_variable freeCv;

 public:
  ThreadPool(std::size_t threadNum) : threadNum(threadNum) {
    threadList.reserve(threadNum);
  }

  std::future<RetType> post(const std::function<Func>& callable) noexcept {
    Task task(callable);
    auto future = task.get_future();
    post(std::move(task));
    return future;
  }

  void post(Task&& task, bool newThreadHint = true) {
    std::lock_guard lock(mutex);
    if (isTerminated) throw IsTerminated("posting new task");
    taskQueue.push(std::move(task));
    if (threadList.size() < threadNum && freeThread < 1 &&
        (newThreadHint || threadList.size() == 0)) {
      threadList.emplace_back(&ThreadPool::run, this);
    }
    workCv.notify_one();
  }

  void wait() {
    std::unique_lock lock(mutex);
    freeCv.wait(
        lock, [&] { return freeThread == threadList.size() || isTerminated; });
  }

  void terminate() {
    std::unique_lock lock(mutex);
    isTerminated = true;
    while (!taskQueue.empty()) {
      taskQueue.pop();
    }
    workCv.notify_all();
    freeCv.notify_all();
  }

  ~ThreadPool() {
    terminate();
    for (auto& thread : threadList) {
      thread.join();
    }
  }

 private:
  void run() {
    std::unique_lock lock(mutex);
    while (!isTerminated) {
      if (!taskQueue.empty()) {
        thread_local Task task;
        task = std::move(taskQueue.front());
        taskQueue.pop();
        lock.unlock();
        task(*this);
        lock.lock();
      }
      if (taskQueue.empty()) {
        freeThread++;
        freeCv.notify_all();
        workCv.wait(lock, [&] { return !taskQueue.empty() || isTerminated; });
        freeThread--;
      }
    }
  }
};
}  // namespace makeDotCpp
