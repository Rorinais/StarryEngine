#!/bin/bash
# ========================================
#   StarryEngine 增量构建脚本（不删目录）
# ========================================
#  用法:
#     ./script/build.sh [release|debug] [-j N] [-d DIR] [-t TARGET]
#     BUILD_DIR=xxx ./script/build.sh       # 或环境变量指定构建目录
#
#  每次先重跑 cmake configure（保持编译数据库 compile_commands.json 最新，
#  IDE 智能提示依赖它），再做增量编译。默认 Release。产物在 <构建目录>/bin/。
#  不指定 -t 时构建全部目标。

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_ROOT" || exit 1

BUILD_TYPE="Release"
BUILD_DIR="${BUILD_DIR:-}"
JOBS="$(nproc 2>/dev/null || echo 4)"
TARGET=""

usage() {
    cat <<EOF
用法: $0 [release|debug] [-j N] [-d DIR] [-t TARGET]
  release|debug  构建类型（默认 release）
  -j N           并行数（默认 = CPU 核数）
  -d DIR         构建目录（默认 build/release 或 build/debug，按类型；可用环境变量 BUILD_DIR）
  -t TARGET      只构建指定目标（demo 名，如 DeferredRender_demo / test1 /
                 TransparentWindow_demo / InferenceChain_demo）；省略则构建全部。
                 产物进 <构建目录>/<TARGET>/（单一构建树，静态库只编一次）
EOF
}

while [[ $# -gt 0 ]]; do
    case $1 in
        release|Release|RELEASE|-release|-Release|-RELEASE) BUILD_TYPE="Release"; shift ;;
        debug|Debug|DEBUG|-debug|-Debug|-DEBUG)              BUILD_TYPE="Debug";   shift ;;
        -j|--jobs)               JOBS="$2"; shift 2 ;;
        -j[0-9]*)                JOBS="${1:2}"; shift ;;
        -d|--dir)                BUILD_DIR="$2"; shift 2 ;;
        -d*)                     BUILD_DIR="${1:2}"; shift ;;
        -t|--target)             TARGET="$2"; shift 2 ;;
        -t*)                     TARGET="${1:2}"; shift ;;
        -h|--help)               usage; exit 0 ;;
        *) echo "未知参数: $1"; usage; exit 1 ;;
    esac
done

# 未指定构建目录时，按构建类型派生（build/release 或 build/debug）
if [ -z "$BUILD_DIR" ]; then
    if [ "$BUILD_TYPE" = "Debug" ]; then
        BUILD_DIR="build/debug"
    else
        BUILD_DIR="build/release"
    fi
fi

if [ ! -d "$BUILD_DIR" ]; then
    echo "错误: 构建目录 $BUILD_DIR 不存在"
    echo "请先运行: ./script/init.sh [-d $BUILD_DIR]"
    exit 1
fi

echo "构建类型: $BUILD_TYPE"
echo "构建目录: $BUILD_DIR"
echo "并行数:   $JOBS"
[ -n "$TARGET" ] && echo "目标:     $TARGET"

# 重跑 configure：既保证选项最新，也刷新 compile_commands.json（IDE 用）
echo "==> 检查 CMake 配置..."
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" || exit 1

echo "==> 增量构建..."
if [ -n "$TARGET" ]; then
    cmake --build "$BUILD_DIR" --target "$TARGET" -j "$JOBS" || exit 1
else
    cmake --build "$BUILD_DIR" -j "$JOBS" || exit 1
fi

echo "========================================"
echo "       增量构建成功!"
echo "========================================"
