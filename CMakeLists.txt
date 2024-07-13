cmake_minimum_required(VERSION 3.28)
project(make-dot-cpp CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
add_compile_options(-Wall)
add_compile_options(-march=native -fprebuilt-module-path=C:/Users/Richard/Package/boost/lib)

# std module
include(FetchContent)
FetchContent_Declare(
  std
  URL "C:/Users/Richard/App/src/llvm-project/build/modules/c++/v1"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  SYSTEM
)
FetchContent_MakeAvailable(std)

# Packages
add_subdirectory("packages/glob" EXCLUDE_FROM_ALL)
find_package(Boost REQUIRED CONFIG
  COMPONENTS filesystem json program_options
)

# Build lib
add_library(makeDotCpp SHARED $<TARGET_OBJECTS:Glob>)
file(GLOB_RECURSE SRC_FILES "src/**/*.cppm")
target_sources(makeDotCpp
  PUBLIC FILE_SET CXX_MODULES
  FILES src/module.cppm ${SRC_FILES}
)
target_link_libraries(makeDotCpp PRIVATE std)
target_include_directories(makeDotCpp PRIVATE src/utils)
target_include_directories(makeDotCpp PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(makeDotCpp PRIVATE ${Boost_LIBRARIES} $<$<BOOL:${WIN32}>:ws2_32>)
target_link_libraries(makeDotCpp PRIVATE Glob)

# Build exe
add_executable(make.cpp src/main.cpp)
target_link_libraries(make.cpp PRIVATE std)
target_link_libraries(make.cpp PRIVATE makeDotCpp)
target_include_directories(make.cpp PRIVATE src/utils)
target_link_libraries(make.cpp PRIVATE ${Boost_LIBRARIES})
target_link_options(make.cpp PRIVATE C:/Users/Richard/Package/boost/lib/boost.dll.o C:/Users/Richard/Package/boost/lib/boost.program_options.o C:/Users/Richard/Package/boost/lib/boost.json.o)

# Install
install(TARGETS makeDotCpp CXX_MODULES_BMI DESTINATION lib)
install(TARGETS make.cpp)
