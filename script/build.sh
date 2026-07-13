#!/bin/bash

# ========================================
#       增量构建脚本（不删除目录）
# ========================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_ROOT" || exit 1

# 设置默认构建类型
BUILD_TYPE="Debug"

# 解析命令行参数
if [ $# -ge 1 ]; then
    case $1 in
        debug|Debug|DEBUG)
            BUILD_TYPE="Debug"
            ;;
        release|Release|RELEASE)
            BUILD_TYPE="Release"
            ;;
        clean)
            echo "执行清理构建..."
            if [ -d "build" ]; then
                cd build
                make clean
                cd ..
                echo "清理完成"
                exit 0
            else
                echo "错误: build 目录不存在"
                exit 1
            fi
            ;;
        *)
            echo "未知参数: $1"
            echo "用法: $0 [debug|release|clean]"
            exit 1
            ;;
    esac
fi

echo "构建类型: $BUILD_TYPE"

# 检查 build 目录是否存在
if [ ! -d "build" ]; then
    echo "错误: build 目录不存在"
    echo "请先运行完整构建脚本或创建 build 目录"
    echo "运行: ./init.sh"
    exit 1
fi

# 进入 build 目录
cd build || exit 1
echo "当前工作目录: $(pwd)"

# 重新运行 CMake 以确保配置正确
echo "检查 CMake 配置..."
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE ..

# 检测是否使用 Ninja 构建系统
if command -v ninja &> /dev/null && [ -f "build.ninja" ]; then
    echo "使用 Ninja 进行增量构建..."
    BUILD_CMD="ninja"
else
    echo "使用 Make 进行增量构建..."
    BUILD_CMD="make -j4"
fi

# 执行增量构建
echo "执行增量构建..."
$BUILD_CMD

if [ $? -ne 0 ]; then
    echo "编译失败!"
    cd ..
    exit 1
fi

echo "========================================"
echo "       增量构建成功!"
echo "========================================"
cd ..