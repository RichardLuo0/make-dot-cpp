export class ObjBuilder : public Builder {
 protected:
  using ModuleMap = std::unordered_map<std::string, Ref<const Target>>;

  auto buildObjTargetList(ModuleMap &moduleMap) const {
    const auto unitList = buildUnitList();
    std::deque<std::unique_ptr<Target>> objList;
    std::vector<std::pair<Ref<ObjTarget>, Ref<const Unit>>> targetList;
    targetList.reserve(unitList.size());
    // Create ObjTarget;
    for (auto &unit : unitList) {
      std::unique_ptr<ObjTarget> obj;
      const auto objPath = absoluteProximate(unit.input) += ".obj";
      if (unit.exported) {
        obj = std::make_unique<ObjTarget>(
            unit.input, unit.includeDeps, objPath,
            replace(unit.moduleName, ':', '-') + ".pcm");
        moduleMap.emplace(unit.moduleName, obj->getPCM());
      } else
        obj =
            std::make_unique<ObjTarget>(unit.input, unit.includeDeps, objPath);
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
          bool isFound = false;
          for (auto &ex : exSet) {
            const auto pcmOpt = ex->findPCM(dep);
            if (pcmOpt.has_value()) {
              const auto &pcm = pcmOpt.value();
              moduleMap.emplace(dep, pcm);
              target.dependOn(pcm);
              isFound = true;
              break;
            }
          }
          if (!isFound) throw ModuleNotFound(unit.input, dep);
        }
      }
    }
    return objList;
  }

  auto buildObjTargetList() const {
    ModuleMap moduleMap;
    return buildObjTargetList(moduleMap);
  }
};
