#!/bin/bash

# ========================================
#       智能运行脚本
# ========================================

# 获取脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# 设置默认构建类型和可执行文件
BUILD_TYPE="Debug"
BUILD_NEEDED=0
BUILD_CLEAN=0

# 可执行文件路径（根据你的CMake配置）
EXECUTABLE_PATH="$SCRIPT_DIR/build/bin/StarryEngine/StarryEngine"

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        debug|Debug|DEBUG)
            BUILD_TYPE="Debug"
            shift
            ;;
        release|Release|RELEASE)
            BUILD_TYPE="Release"
            shift
            ;;
        -f|--force)
            BUILD_NEEDED=1
            shift
            ;;
        -c|--clean)
            BUILD_CLEAN=1
            shift
            ;;
        -p|--path)
            if [[ -n $2 ]]; then
                EXECUTABLE_PATH=$2
                shift 2
            else
                echo "错误: --path 需要指定路径"
                exit 1
            fi
            ;;
        -h|--help)
            echo "用法: $0 [选项]"
            echo "选项:"
            echo "  debug|release       构建类型 (默认: debug)"
            echo "  -p, --path PATH     可执行文件路径"
            echo "  -f, --force         强制重新构建"
            echo "  -c, --clean         清理后构建"
            echo "  -h, --help          显示此帮助信息"
            exit 0
            ;;
        *)
            echo "未知参数: $1"
            echo "使用 --help 查看帮助信息"
            exit 1
            ;;
    esac
done

echo "运行类型: $BUILD_TYPE"
echo "可执行文件路径: $EXECUTABLE_PATH"

# 检查是否需要构建
if [ $BUILD_NEEDED -eq 0 ]; then
    # 检查可执行文件是否存在
    if [ ! -f "$EXECUTABLE_PATH" ]; then
        echo "可执行文件不存在，需要重新构建..."
        BUILD_NEEDED=1
    else
        # 获取可执行文件的修改时间
        LAST_BUILD_TIME=$(stat -c %Y "$EXECUTABLE_PATH" 2>/dev/null || stat -f %m "$EXECUTABLE_PATH")
        
        echo "最后构建时间: $(date -d @$LAST_BUILD_TIME '+%Y-%m-%d %H:%M:%S')"
        
        # 检查源代码目录中的文件是否比可执行文件新
        echo "检查源代码修改情况..."
        
        # 使用 while 循环避免子shell问题
        # 查找所有源代码文件，使用 -exec 和 sh -c 来避免管道
        find "$SCRIPT_DIR/src" -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" -o -name "*.c" -o -name "*.cxx" -o -name "*.cc" -o -name "*.ixx" \) 2>/dev/null \
            -exec sh -c '
                for file do
                    FILE_TIME=$(stat -c %Y "$file" 2>/dev/null || stat -f %m "$file")
                    if [ "$FILE_TIME" -gt "$1" ]; then
                        echo "检测到源代码更改: $(basename "$file")"
                        exit 1
                    fi
                done
                exit 0
            ' _ "$LAST_BUILD_TIME" {} +
        
        if [ $? -eq 1 ]; then
            BUILD_NEEDED=1
        fi
        
        # 检查资源文件是否更改
        if [ $BUILD_NEEDED -eq 0 ] && [ -d "$SCRIPT_DIR/assets" ]; then
            echo "检查资源文件修改情况..."
            
            # 定义资源文件扩展名数组
            ASSET_EXTS=(
                "*.glsl" "*.vert" "*.frag" "*.comp"
                "*.png" "*.jpg" "*.jpeg"
                "*.obj" "*.fbx"
                "*.wav" "*.mp3"
                "*.ttf"
                "*.json" "*.xml"
            )
            
            # 检查每个扩展名类型的文件
            for ext in "${ASSET_EXTS[@]}"; do
                find "$SCRIPT_DIR/assets" -type f -name "$ext" 2>/dev/null \
                    -exec sh -c '
                        for file do
                            FILE_TIME=$(stat -c %Y "$file" 2>/dev/null || stat -f %m "$file")
                            if [ "$FILE_TIME" -gt "$1" ]; then
                                echo "检测到资源文件更改: $(basename "$file")"
                                exit 1
                            fi
                        done
                        exit 0
                    ' _ "$LAST_BUILD_TIME" {} +
                
                if [ $? -eq 1 ]; then
                    BUILD_NEEDED=1
                    break
                fi
            done
        fi
    fi
fi

# 如果需要构建
if [ $BUILD_NEEDED -eq 1 ]; then
    echo
    echo "需要重新构建项目..."
    
    # 确定构建选项
    if [ $BUILD_CLEAN -eq 1 ]; then
        echo "执行清理构建..."
        "$SCRIPT_DIR/build.sh" clean
    fi
    
    # 调用构建脚本
    if [ $BUILD_CLEAN -eq 1 ] || [ ! -d "$SCRIPT_DIR/build" ]; then
        echo "执行完整构建..."
        "$SCRIPT_DIR/init.sh" "$BUILD_TYPE"
    else
        echo "执行增量构建..."
        "$SCRIPT_DIR/build.sh" "$BUILD_TYPE"
    fi
    
    if [ $? -ne 0 ]; then
        echo "构建失败，无法运行程序!"
        exit 1
    fi
else
    echo "没有检测到更改，使用现有构建..."
fi

# 检查可执行文件是否存在
if [ ! -f "$EXECUTABLE_PATH" ]; then
    echo "错误: 可执行文件不存在: $EXECUTABLE_PATH"
    echo
    echo "尝试查找可执行文件..."
    find "$SCRIPT_DIR/build" -name "StarryEngine" -type f 2>/dev/null
    echo
    echo "请检查构建输出或 CMakeLists.txt 配置"
    exit 1
fi

# 检查文件是否可执行
if [ ! -x "$EXECUTABLE_PATH" ]; then
    echo "警告: 文件没有执行权限，尝试添加..."
    chmod +x "$EXECUTABLE_PATH"
fi

# 运行可执行文件
echo "正在运行程序..."
cd "$(dirname "$EXECUTABLE_PATH")" || exit 1
"./$(basename "$EXECUTABLE_PATH")"

EXIT_CODE=$?

echo
echo "程序退出，代码: $EXIT_CODE"