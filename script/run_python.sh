#!/bin/bash
# ========================================
#  运行 StarryEngine Python 绑定
#  用法: ./script/run_python.sh [py脚本路径，默认 python/demo.py]
# ========================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_ROOT" || exit 1

if ! compgen -G "build/bin/python/starryengine_py.cpython-*.so" > /dev/null; then
    echo "错误: Python 模块未编译，请先运行 ./script/build.sh"
    exit 1
fi

export LD_LIBRARY_PATH="$PROJECT_ROOT/build/bin:$LD_LIBRARY_PATH"
export VK_LAYER_PATH="$PROJECT_ROOT/build/bin/layers"
export PYTHONPATH="$PROJECT_ROOT/build/bin/python"

# 优先使用项目捆绑的便携 Python（external/python），不依赖系统环境；找不到则回退系统 python3
PY_BIN="${PROJECT_ROOT}/external/python/bin/python3.13"
if [ ! -x "$PY_BIN" ]; then
    PY_BIN="$(command -v python3)"
    echo "[提示] 未找到捆绑 Python，回退系统: $PY_BIN"
fi

TARGET="${1:-python/demo.py}"
echo "运行: $TARGET"
"$PY_BIN" "$TARGET"
