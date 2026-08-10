#!/bin/bash
# ========================================
#   StarryEngine 智能运行脚本（需要时构建 + 运行）
# ========================================
#  用法:
#     ./script/run.sh [release|debug] [-f] [-d DIR] [-t TARGET] [-- 程序参数...]
#     BUILD_DIR=xxx ./script/run.sh       # 或环境变量指定构建目录
#
#  自动判断: 可执行文件缺失 / --force / 源码或资源更新 → 先增量构建再运行。
#  demo 以相对路径加载资源，故在 bin/ 目录下执行。默认 Release。
#  -t 指定 demo 目标（默认 DeferredRender_demo）。

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_ROOT" || exit 1

BUILD_TYPE="Release"
BUILD_DIR="${BUILD_DIR:-}"
TARGET="${STARRY_TARGET:-DeferredRender_demo}"
FORCE=0
APP_ARGS=()

usage() {
    cat <<EOF
用法: $0 [release|debug] [-f] [-d DIR] [-t TARGET] [-- 程序参数...]
  release|debug  构建类型（默认 release）
  -f, --force    强制重新构建（忽略时间戳）
  -d DIR         构建目录（默认 build/release 或 build/debug，按类型；可用环境变量 BUILD_DIR）
  -t TARGET      demo 目标名（默认 DeferredRender_demo；可选 test1 /
                 TransparentWindow_demo / InferenceChain_demo）。
                 产物进 <构建目录>/<TARGET>/（单一构建树，静态库只编一次）
  -- args        其后参数原样传给程序
EOF
}

while [[ $# -gt 0 ]]; do
    case $1 in
        release|Release|RELEASE|-release|-Release|-RELEASE) BUILD_TYPE="Release"; shift ;;
        debug|Debug|DEBUG|-debug|-Debug|-DEBUG)              BUILD_TYPE="Debug";   shift ;;
        -f|--force)              FORCE=1; shift ;;
        -d|--dir)                BUILD_DIR="$2"; shift 2 ;;
        -d*)                     BUILD_DIR="${1:2}"; shift ;;
        -t|--target)             TARGET="$2"; shift 2 ;;
        -t*)                     TARGET="${1:2}"; shift ;;
        -h|--help)               usage; exit 0 ;;
        --)                      shift; APP_ARGS=("$@"); break ;;
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

# 单一构建树 + 每项目独立目录（与项目同名）：exe 在 <build>/<TARGET>/<TARGET>
EXECUTABLE_PATH="$PROJECT_ROOT/$BUILD_DIR/$TARGET/$TARGET"

# ---- 判断是否需要构建 ----
NEED_BUILD=0
if [ $FORCE -eq 1 ]; then
    NEED_BUILD=1
elif [ ! -f "$EXECUTABLE_PATH" ]; then
    NEED_BUILD=1
else
    LAST_BUILD_TIME=$(stat -c %Y "$EXECUTABLE_PATH" 2>/dev/null)
    # 源码或资源比可执行文件新 → 重建
    if find "$PROJECT_ROOT/src" "$PROJECT_ROOT/demo" \
        -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" -o -name "*.c" \
                   -o -name "*.cxx" -o -name "*.cc" \) \
        -newer "$EXECUTABLE_PATH" -print -quit 2>/dev/null | grep -q .; then
        echo "检测到源码更改，需要重新构建..."
        NEED_BUILD=1
    elif [ -d "$PROJECT_ROOT/assets" ] && \
        find "$PROJECT_ROOT/assets" -type f \
            \( -name "*.vert" -o -name "*.frag" -o -name "*.comp" -o -name "*.glsl" \
               -o -name "*.png" -o -name "*.jpg" -o -name "*.jpeg" \
               -o -name "*.fbx" -o -name "*.obj" -o -name "*.gltf" -o -name "*.glb" \
               -o -name "*.ttf" -o -name "*.json" \) \
            -newer "$EXECUTABLE_PATH" -print -quit 2>/dev/null | grep -q .; then
        echo "检测到资源更改，需要重新构建..."
        NEED_BUILD=1
    fi
fi

# ---- 构建 ----
if [ $NEED_BUILD -eq 1 ]; then
    if [ ! -d "$PROJECT_ROOT/$BUILD_DIR" ]; then
        echo "==> 首次构建（目录不存在）..."
        "$SCRIPT_DIR/init.sh" "$BUILD_TYPE" -d "$BUILD_DIR" -t "$TARGET"
    else
        "$SCRIPT_DIR/build.sh" "$BUILD_TYPE" -d "$BUILD_DIR" -t "$TARGET"
    fi
    if [ $? -ne 0 ]; then
        echo "构建失败，无法运行程序!"
        exit 1
    fi
else
    echo "没有检测到更改，使用现有构建..."
fi

if [ ! -f "$EXECUTABLE_PATH" ]; then
    echo "错误: 可执行文件不存在: $EXECUTABLE_PATH"
    exit 1
fi

# ---- 运行（在 bin/ 目录下，资源相对路径加载）----
echo "正在运行程序: $BUILD_DIR/$TARGET/$TARGET ${APP_ARGS[*]}"
cd "$(dirname "$EXECUTABLE_PATH")" || exit 1
"./$(basename "$EXECUTABLE_PATH")" "${APP_ARGS[@]}"
EXIT_CODE=$?

echo
echo "程序退出，代码: $EXIT_CODE"
