export module makeDotCpp.builder:ObjBuilder;
import :Builder;
import :Targets;
import std;
import makeDotCpp;
import makeDotCpp.utils;

#include "alias.hpp"
#include "macro.hpp"

namespace makeDotCpp {
export defException(ModuleNotFound,
                    (const Path input, const std::string &moduleName),
                    input.generic_string() +
                        ": module not found: " + moduleName);

export class ObjBuilder : public Builder {
 protected:
  using ModuleMap = std::unordered_map<std::string, Ref<const ModuleTarget>>;

  std::optional<Ref<const ModuleTarget>> findPCM(
      const std::string &moduleName) const {
    for (auto &ex : exSet) {
      const auto pcmOpt = ex->findPCM(moduleName);
      if (pcmOpt.has_value()) return pcmOpt.value();
    }
    return std::nullopt;
  }

  auto buildObjTargetList(const Context &ctx, ModuleMap &moduleMap) const {
    const auto inputInfo = buildInputInfo();
    const auto unitList = buildUnitList(ctx, inputInfo);
    const auto &base = inputInfo.second;
    std::deque<std::unique_ptr<Target>> objList;
    std::vector<std::pair<Ref<ObjTarget>, Ref<const Unit>>> targetList;
    targetList.reserve(unitList.size());
    // Create ObjTarget
    for (auto &unit : unitList) {
      std::unique_ptr<ObjTarget> obj;
      const auto objPath = fs::proximate(unit.input, base) += ".obj";
      if (unit.exported) {
        obj = std::make_unique<ObjTarget>(
            unit.input, unit.includeDeps, objPath, unit.moduleName,
            replace(unit.moduleName, ':', '-') + ".pcm");
        moduleMap.emplace(unit.moduleName, obj->getPCM());
      } else
        obj =
            std::make_unique<ObjTarget>(unit.input, unit.includeDeps, objPath);
      obj->dependOn(getCompileOptionsJson(ctx));
      targetList.emplace_back(*obj, unit);
      objList.emplace_back(std::move(obj));
    }
    // Create depends with modules
    for (auto &pair : targetList) {
      auto &target = pair.first.get();
      const auto &unit = pair.second.get();
      for (auto &dep : unit.moduleDeps) {
        const auto it = moduleMap.find(dep);
        if (it != moduleMap.end())
          target.dependOn(it->second);
        else {
          const auto pcmOpt = findPCM(dep);
          if (pcmOpt.has_value()) {
            const auto pcm = pcmOpt.value();
            moduleMap.emplace(dep, pcm);
            target.dependOn(pcm);
          } else
            throw ModuleNotFound(unit.input, dep);
        }
      }
    }
    return objList;
  }

  auto buildObjTargetList(const Context &ctx) const {
    ModuleMap moduleMap;
    return buildObjTargetList(ctx, moduleMap);
  }

 public:
  using Builder::Builder;
};
}  // namespace makeDotCpp
