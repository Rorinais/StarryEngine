# StarryEngine

Vulkan 渲染引擎（C++20，Linux / Windows）。依赖**全自包含**（vendored），clone 后即可构建，无任何必需系统包。

## 系统依赖

| 平台 | 必需系统包 | 说明 |
|---|---|---|
| **Linux** | **无** | assimp 用 vendored 源码静态构建（5.4.3，含内置 zlib，二进制 FBX 可解压），Vulkan/glfw/glm/spdlog/shaderc 等全部 vendored |
| **Linux（可选）** | Vulkan loader/头、glfw3、glm、spdlog、assimp | 系统装过则在 `STARRY_PREFER_VENDORED=OFF` 时自动优先使用（find_package），否则回退 vendored，零影响 |
| **Windows** | 无（Vulkan 驱动 + 安装 [Vulkan SDK](https://vulkan.lunarg.com/sdk/home) 加载器即可） | assimp 用 vendored MSVC dll（实测 5.4.3.0，与内嵌头配套，动画正确）；glfw/Vulkan/shaderc 均 vendored |

> MinGW 工具链的 vendored assimp 是 6.0.2 dll，与 5.4.3 头不完全配套（动画可能异常）。
> 优先用 MSVC 工具链。

## 构建

推荐用 `script/` 下的脚本（自包含、默认 Release，不需要装任何系统包），
`.sh` 与 `.bat` 参数一致：

脚本统一用法：`release|debug` 选构建类型（默认 **release**）、`-d DIR` 指定构建目录
（默认按类型 `build/release` 或 `build/debug`，`.sh` 也可用环境变量 `BUILD_DIR`）、
`-j N` 并行数、`-t TARGET` 指定 demo 目标（默认全部，`run` 默认 `DeferredRender_demo`）。
例如：`BUILD_DIR=build11 ./script/build.sh release -t test1`。

**多项目（单一构建树 + 每项目独立目录）**：所有 demo 共享一个构建树，静态库
（core/renderer/...）只编译一次；每个项目的产物自包含在 `build/<release|debug>/<项目名>/` 下
（可执行文件 + assets + DLL + 验证层）。release 与 debug 分目录，互不干扰。`-t TARGET`
只构建指定 demo，产物进 `<构建目录>/<TARGET>/`；`-d DIR` / `BUILD_DIR` 可显式指定构建树。
不传 `-t` 构建全部。

Linux：

```bash
./script/init.sh          # 全新清理构建（release 产物在 build/release/<demo>/）
./script/init.sh debug    # debug 产物在 build/debug/<demo>/（含验证层）
./script/build.sh         # 增量构建（不删目录，每次刷新编译数据库）
./script/run.sh -t test1  # 需要时自动增量构建 + 运行指定 demo
```

Windows（cmd，MSVC；Visual Studio 生成器下 `--config` 选配置）：

```bat
init.bat            :: 全新清理构建（release 产物在 build\release\<demo>\）
init.bat debug      :: debug 产物在 build\debug\<demo>\
build.bat -t test1  :: 增量构建指定 demo
run.bat             :: 需要时自动增量构建 + 运行（默认 DeferredRender_demo）
```

等价的原生命令：

```bash
# Linux
cmake -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release -j$(nproc)

# Windows (cmd)
cmake -B build\release -DCMAKE_BUILD_TYPE=Release
cmake --build build\release --config Release
```

每个项目的产物自包含在 `build/<release|debug>/<项目名>/`（如 `build/release/DeferredRender_demo/`，
可执行文件 + 完整 `assets/` 副本）。demo 以相对路径加载资源，运行时工作目录即该目录
（`script/run.sh` 自动处理）：

```bash
cd build/release/DeferredRender_demo
./DeferredRender_demo                 # 默认 warn 静默日志
STARRY_VERBOSE=1 ./DeferredRender_demo  # 看帧数据/动画键统计
```

### 可选配置

- `-DSTARRY_PREFER_VENDORED=OFF`：改为"系统优先"——系统装了对应包就用系统（Vulkan/glfw/glm/spdlog/assimp 等），找不到再回退 vendored。默认 `ON`（一律 vendored，自包含、可复现）。
- `-DCMAKE_BUILD_TYPE=Debug`：启用 Vulkan 验证层（复制到每个项目的输出目录 `<demo>/`）。注意**默认是 Release**（空构建类型会导致验证层被请求而崩溃）；Debug 需要验证层与驱动可用，一般开发用 Release 即可。
- `CMAKE_EXPORT_COMPILE_COMMANDS`：默认开启，构建目录生成 `compile_commands.json`（IDE 智能提示用）。

### IDE / 智能提示（VSCode）

项目已内置 IntelliSense 配置，开箱即用：

- **编译数据库**：根 `CMakeLists.txt` 默认导出 `compile_commands.json` 到构建目录。若 IDE 需要，把根目录软链指到当前构建目录：
  ```bash
  ln -sf build/compile_commands.json compile_commands.json
  ```
- **`.vscode/c_cpp_properties.json`**：已写好引擎模块 + vendored 头的 `includePath` 与关键宏（`GLFW_INCLUDE_NONE`、`FMT_HEADER_ONLY`、`SPDLOG_HEADER_ONLY` 等）。若构建目录不是 `build/`，把文件里 assimp 的两条 `build/_deps/...` 路径改成实际目录，然后运行 "C/C++: Reset IntelliSense Database"。

## 依赖解析策略（vendored 伪包）

每个依赖都是一个 CMake 包：vendored 依赖在 `external/<dep>/lib/cmake/<Pkg>/` 提供自己的
`*Config.cmake` 伪包（与系统包**同名、同 target**），`cmake/Dependencies.cmake` 全部用
`find_package` 统一消费——没有手写 IMPORTED target / add_subdirectory 兜底。这也是
"把 SDK 扔进项目"的形态：整个 Vulkan SDK 子集（loader/头/shaderc/spirv-cross/
验证层）已 vendored 在 `external/vulkan/`。

- **Vulkan**：先找 vendored CONFIG，失败回退系统 FindVulkan 模块。
- **assimp**：Linux 用 vendored 源码静态构建——`external/assimp/assimp-5.4.3.tar.gz`
  是裁剪版 5.4.3 源码（去掉 test/samples/doc/tools 及 draco/googletest，保留全部
  code/include 与内置 zlib），FetchContent 构建期静态编译，与 5.4.3 头严格同版
  （动画键 ABI 正确，0 坏键）。Windows 走 vendored dll 伪包。
- **shaderc / spirv-cross**：无系统包，固定 vendored 伪包。
- **单头库**（stb_image / vk_mem_alloc / nlohmann_json）：无系统包，归拢在
  `external/header_libs/` 一个共享目录，每个仍是独立 INTERFACE 伪包。
- **imgui**：vendored 伪包——config 在 find_package 时把 `src/` 源码编译成静态库
  （含 glfw/vulkan 背板与 ImGuiFileDialog/TextEditor），头在 `include/`。

## 目录结构

```
cmake/Dependencies.cmake   依赖解析（vendored 伪包 + 统一 find_package）
cmake/EngineHelpers.cmake  add_engine_executable（资源复制 / DLL 复制 / RPATH / 验证层）
script/                              构建/运行脚本（init 全量 / build 增量 / run 运行；.sh 与 .bat 各一份，参数一致）
.vscode/c_cpp_properties.json        VSCode C++ 智能提示配置（includePath + 宏）
src/<模块>/include/<模块>/                公共头（include 写作 <模块/...>，如 <renderer/RenderTypes.hpp>）
src/<模块>/src/                           实现
external/<dep>/include/  +  lib/          vendored 依赖官方安装布局（头 include/，库/伪包 lib/cmake/）
external/vulkan/                         vendored Vulkan SDK 子集
external/header_libs/                    单头库合集（stb_image / vk_mem_alloc / nlohmann_json）
demo/                                    demo 可执行程序
assets/                                  资源（shaders/models/textures/...）
```
