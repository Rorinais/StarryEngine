# 项目构建和运行指南

## 脚本概述

本项目提供了三个批处理脚本来简化构建和运行流程：

- `init.bat` - 初始化项目，执行完整清理构建
- `build.bat` - 执行增量构建（默认不清理）
- `run.bat` - 智能检测更改并运行程序

## 使用说明

```batch
# 初始化项目（完整清理构建）
init.bat

# 运行程序
run.bat

init.bat                     # 初始化 Release 版本，使用默认作业数(4)
init.bat debug               # 初始化 Debug 版本，使用默认作业数(4)
init.bat release             # 初始化 Release 版本，使用默认作业数(4)
init.bat jobs 8              # 初始化 Release 版本，使用8个并行作业
init.bat debug jobs 6        # 初始化 Debug 版本，使用6个并行作业
init.bat release jobs 2      # 初始化 Release 版本，使用2个并行作业

# 基本构建命令
build.bat                    # 增量构建 Release 版本，使用默认作业数(4)
build.bat debug              # 增量构建 Debug 版本，使用默认作业数(4)
build.bat release            # 增量构建 Release 版本，使用默认作业数(4)

# 清理构建命令
build.bat clean              # 清理构建 Release 版本，使用默认作业数(4)
build.bat debug clean        # 清理构建 Debug 版本，使用默认作业数(4)
build.bat release clean      # 清理构建 Release 版本，使用默认作业数(4)

# 明确指定增量构建
build.bat noclean            # 增量构建 Release 版本，使用默认作业数(4)
build.bat debug noclean      # 增量构建 Debug 版本，使用默认作业数(4)
build.bat release noclean    # 增量构建 Release 版本，使用默认作业数(4)

# 控制并行作业数
build.bat jobs 8             # 增量构建 Release 版本，使用8个并行作业
build.bat debug jobs 6       # 增量构建 Debug 版本，使用6个并行作业
build.bat release jobs 2     # 增量构建 Release 版本，使用2个并行作业

# 组合使用
build.bat clean jobs 8       # 清理构建 Release 版本，使用8个并行作业
build.bat debug clean jobs 4 # 清理构建 Debug 版本，使用4个并行作业
build.bat noclean jobs 6     # 增量构建 Release 版本，使用6个并行作业

# 基本运行命令
run.bat                     # 运行 Release 版本，自动检测更改
run.bat debug               # 运行 Debug 版本，自动检测更改
run.bat release             # 运行 Release 版本，自动检测更改

# 强制重新构建命令
run.bat force               # 强制重新构建并运行 Release 版本
run.bat debug force         # 强制重新构建并运行 Debug 版本
run.bat release force       # 强制重新构建并运行 Release 版本

# 清理构建命令
run.bat clean               # 使用清理构建并运行 Release 版本
run.bat debug clean         # 使用清理构建并运行 Debug 版本
run.bat release clean       # 使用清理构建并运行 Release 版本

# 指定可执行文件名称
run.bat exe MyApp           # 运行名为 MyApp 的可执行文件
run.bat debug exe MyApp     # 运行 Debug 版本，名为 MyApp 的可执行文件
run.bat release exe MyApp   # 运行 Release 版本，名为 MyApp 的可执行文件

# 指定可执行文件路径
run.bat path "build/bin/MyApp"          # 运行指定路径的可执行文件
run.bat debug path "build/bin/MyApp"    # 运行 Debug 版本，指定路径的可执行文件
run.bat release path "build/bin/MyApp"  # 运行 Release 版本，指定路径的可执行文件

# 组合使用
run.bat debug exe MyApp path "build/bin/MyApp"      # 运行 Debug 版本，指定名称和路径
run.bat release exe MyApp path "build/bin/MyApp"    # 运行 Release 版本，指定名称和路径
run.bat debug force exe MyApp                       # 强制重新构建并运行 Debug 版本，指定名称
run.bat release clean exe MyApp                     # 使用清理构建并运行 Release 版本，指定名称
run.bat debug force clean exe MyApp path "build/bin/MyApp"  # 强制清理构建并运行 Debug 版本，指定名称和路径# StarryEngine
