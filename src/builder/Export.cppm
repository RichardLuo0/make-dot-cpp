export module makeDotCpp.builder:Export;
import :Targets;
import std;
import makeDotCpp;

#include "alias.hpp"

namespace makeDotCpp {
export struct Export {
  virtual ~Export() = default;

  virtual std::string getCompileOption() const { return std::string(); }

  virtual std::string getLinkOption() const { return std::string(); }

  virtual std::optional<Ref<const ModuleTarget>> findModule(
      [[maybe_unused]] const std::string& moduleName) const {
    return std::nullopt;
  }

  virtual std::optional<Ref<const Target>> getTarget() const {
    return std::nullopt;
  }
};

export struct ExportFactory {
  virtual ~ExportFactory() = default;

  // This is used for user build file to change behaviours in export.
  virtual void set([[maybe_unused]] const std::string& key,
                   [[maybe_unused]] const std::string& value) {}
  virtual std::shared_ptr<Export> create(const Context& ctx) const = 0;
};

export struct CachedExportFactory : public ExportFactory {
 private:
  mutable std::unordered_map<const Context*, std::weak_ptr<Export>> cache;

 public:
  std::shared_ptr<Export> create(const Context& ctx) const override {
    auto it = cache.find(&ctx);
    if (it != cache.end())
      if (auto ex = it->second.lock()) return ex;
    return cache.emplace(&ctx, onCreate(ctx)).first->second.lock();
  };

 protected:
  virtual std::shared_ptr<Export> onCreate(const Context& ctx) const = 0;
};
}  // namespace makeDotCpp
