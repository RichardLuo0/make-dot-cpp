# make.cpp
Yet another build system for C++.\
This is a build system that uses cpp to build cpp project. It is highly customizable and is designed with support for C++20 modules in mind.

# Installation
**boost library is mandatory, this may change in the future**
* Download the zip in release section.
* Extract all files into a folder. Think this folder as `node_modules` in js or `site-packages` in python. Assume the folder is named as `packages`.
* Set a env variable `CXX_PACKAGES` and set the value to `packages`.
* Add `packages/makeDotCpp/bin` into `PATH`.
* Follow the steps in [Usage](#Usage). The first build may be slow due to generation of BMI files for makeDotCpp.

# Usage
The following steps maybe simplified in the future with `make.cpp init` command.\
For now only clang compiler is supported to build `build.cpp`. This might change when global config file is supported.
* Create a file named `project.json` under your project.\
The `project.json` in this project is a good example of it. You can find the full description in `src/project/ProjectDesc.cppm` and `src/project/Usage.cppm`.
* Create a file named `build.cpp` (This can be controlled through `dev.buildFile` in `project.json`).
* Add a function:
  ```cpp
  import makeDotCpp.dll.api;

  using namespace makeDotCpp;
  using namespace api;

  extern "C" int build(const ProjectContext &ctx) {

  };
  ```
* The `ProjectContext` will provide all information you will need. You can find the definition in `src/dll/api.cppm`.
* The `build.cpp` in this project is a good example of how to use makeDotCpp to build a project, however if you want you can build the project as you wish.
* Simply type `make.cpp` to init a build. More options can be found in `make.cpp --help`.

# Notice
* All build files and global packages will be build with flags `-march=native -std=c++20 -Wall`. Because the artifacts are not supposed to be shared with others.
* For now only clang is supported to build `build.cpp` and global packages. However, the project may be compiled with other compilers, to do this, you need to inherit from Compiler class in makeDotCpp.compiler to create your own compiler implementation. Compile it as a dll and put the dll under `packages/.global/compiler`. Then you may use `make.cpp --compiler <The name of dll>` to build a project.
* `cmake` and other build system compatibility is not yet implemented. However, to use a package from other build system, you may look at `src/builder/Export.cppm` to create your own implementations.

# Credits
* [boost library](https://github.com/boostorg)
* [glob](https://github.com/p-ranav/glob)
