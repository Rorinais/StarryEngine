#!/bin/bash
# ========================================
#   StarryEngine 全量清理构建脚本（自包含）
# ========================================
#  用法:
#     ./script/init.sh [release|debug] [-j N] [-d DIR] [-t TARGET]
#     BUILD_DIR=xxx ./script/init.sh       # 或环境变量指定构建目录
#
#  项目零系统依赖（vendored 伪包 + assimp 源码构建），本脚本不做任何系统包
#  安装/sudo。唯一系统要求: 可用的 GPU Vulkan 驱动（NVIDIA / Mesa 开源驱动）。
#  默认 Release 构建；Debug 会定义 ENABLE_VALIDATION_LAYERS 并复制验证层，
#  需要验证层与驱动可用，一般开发用 Release 即可。产物在 <构建目录>/bin/。

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
  -t TARGET      只构建指定目标（demo 名）；省略则构建全部
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

echo "构建类型: $BUILD_TYPE"
echo "构建目录: $BUILD_DIR"
echo "并行数:   $JOBS"
[ -n "$TARGET" ] && echo "目标:     $TARGET"

if [ "$BUILD_TYPE" = "Debug" ]; then
    echo "提示: Debug 会启用 Vulkan 验证层（ENABLE_VALIDATION_LAYERS），"
    echo "      若报验证层相关错误，请改用 Release。"
fi

# 全新构建（删目录，保证干净）
rm -rf "$BUILD_DIR"

echo "==> CMake 配置..."
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" || exit 1

echo "==> 全量构建..."
if [ -n "$TARGET" ]; then
    cmake --build "$BUILD_DIR" --target "$TARGET" -j "$JOBS" || exit 1
else
    cmake --build "$BUILD_DIR" -j "$JOBS" || exit 1
fi

echo "========================================"
echo "       全量构建成功!"
echo "========================================"
echo "产物: $BUILD_DIR/<demo>/（每个项目独立目录，exe+assets 自包含；release/debug 分目录）"
echo "运行: ./script/run.sh          （自动增量构建 + 运行，默认 DeferredRender_demo）"
echo "  或: ./script/run.sh -t test1 （指定 demo）"
