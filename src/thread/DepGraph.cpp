template <class T>
class QuickRemoveList : public std::list<T> {
 private:
  using Parent = std::list<T>;

 public:
  struct ItemWrapper {
    friend class QuickRemoveList;

   private:
    Parent::const_iterator it;
  };

  template <class... Args>
  T& emplace_back(Args&&... args) {
    T& item = Parent::emplace_back(std::forward<Args>(args)...);
    item.it = --Parent::end();
    return item;
  }

  void erase(const T& item) { Parent::erase(item.it); }
};

// alg2
// space: O(nE_{out})
// time: O(E_{out})
export class DepGraph {
 private:
  using Func = ThreadPool::RetType(DepGraph&);
  using RetType = ThreadPool::RetType;
  using Task = ThreadPool::Task;

 public:
  struct Node : QuickRemoveList<Node>::ItemWrapper {
    friend class DepGraph;

   private:
    std::deque<Ref<Node>> parents;
    Task task;
    std::future<RetType> future;
    std::size_t children = 0;
    enum { Pending, Running, Finished } state = Pending;

    void dependOn(Node& node) {
      if (node.state != Finished) {
        node.parents.emplace_back(*this);
        children++;
      }
    }

   public:
    Node(DepGraph& graph, const std::function<Func>&& callable)
        : task([&, callable](ThreadPool& threadPool) {
            const auto& ret = callable(graph);
            std::lock_guard lock(graph.mutex);
            state = Finished;
            for (auto& parentRef : parents) {
              auto& parent = parentRef.get();
              parent.children--;
              if (graph.threadPool)
                parent.run(*graph.threadPool, parents.size() != 1);
            }
            graph.removeNode(*this);
            return ret;
          }) {
      future = task.get_future();
    }

    std::future<RetType>&& takeFuture() { return std::move(future); }

    void run(ThreadPool& threadPool, bool newThreadHint = true) {
      if (state == Pending && children == 0) {
        try {
          threadPool.post(std::move(task), newThreadHint);
          state = Running;
        } catch (const ThreadPool::IsTerminatedError& e) {
        }
      }
    }
  };

 private:
  ThreadPool* threadPool = nullptr;
  QuickRemoveList<Node> nodeList;
  std::mutex mutex;

  void removeNode(Node& node) { nodeList.erase(node); }

 public:
  Node& addNode(std::function<Func>&& callable,
                const std::ranges::range auto& depends) noexcept {
    std::lock_guard lock(mutex);
    Node& node = nodeList.emplace_back(*this, std::move(callable));
    for (auto& depend : depends) {
      node.dependOn(depend);
    }
    return node;
  }

  Node& addNode(std::function<Func>&& callable) {
    std::lock_guard lock(mutex);
    return nodeList.emplace_back(*this, std::move(callable));
  }

  void runOn(ThreadPool& threadPool) {
    std::lock_guard lock(mutex);
    this->threadPool = &threadPool;
    for (auto& node : nodeList) {
      node.run(threadPool);
    }
  }

  void stop() {
    std::lock_guard lock(mutex);
    this->threadPool = nullptr;
  }

  void terminate() {
    std::lock_guard lock(mutex);
    nodeList.clear();
    this->threadPool = nullptr;
  }
};
