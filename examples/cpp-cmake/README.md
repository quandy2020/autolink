# Autolink FindPackage Demo

This demo shows how to consume `autolink` from an external CMake project via:

- `find_package(Autolink REQUIRED)`
- `target_link_libraries(your_target PRIVATE Autolink::Autolink)`

## Build

```bash
cmake -S examples/cpp-cmake -B build/cpp-cmake
cmake --build build/cpp-cmake -j
```

## Run

```bash
./build/cpp-cmake/autolink_find_package_demo
```

## Notes

- This repo provides `cmake/FindAutolink.cmake`.  
  The demo `CMakeLists.txt` adds that path to `CMAKE_MODULE_PATH` automatically when used in-tree.
- If you copy this demo out of the repo, make sure CMake can find `FindAutolink.cmake`, for example:

```bash
cmake -S . -B build -DCMAKE_MODULE_PATH=/path/to/autolink/cmake
```

