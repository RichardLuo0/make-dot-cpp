export class FutureList : public std::vector<std::future<ThreadPool::RetType>> {
 public:
  void wait() {
    for (auto& future : *this) {
      future.wait();
    }
  }

  std::vector<ThreadPool::RetType> get() {
    std::vector<ThreadPool::RetType> resultList;
    resultList.reserve(size());
    for (auto& future : *this) {
      try {
        resultList.emplace_back(future.get());
      } catch (const std::future_error& e) {
      }
    }
    return resultList;
  }
};
