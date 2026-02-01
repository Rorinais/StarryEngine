#!/bin/bash

# ========================================
#       项目初始化构建脚本
# 自动检测并安装OpenGL开发库
# ========================================

# 获取脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# 设置默认构建类型
BUILD_TYPE="Debug"
MAKE_JOBS=4

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
        -j|--jobs)
            if [[ -n $2 ]]; then
                MAKE_JOBS=$2
                shift 2
            else
                echo "错误: --jobs 需要指定一个数字"
                exit 1
            fi
            ;;
        --no-deps)
            NO_DEPS=1
            shift
            ;;
        *)
            echo "未知参数: $1"
            echo "用法: $0 [debug|release] [--jobs N] [--no-deps]"
            exit 1
            ;;
    esac
done

echo "构建类型: $BUILD_TYPE"
echo "并行编译数: $MAKE_JOBS"

# 函数：检测并安装依赖
check_and_install_deps() {
    echo "检测系统依赖..."
    
    # 检测系统类型
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        OS=$NAME
    else
        OS=$(uname -s)
    fi
    
    echo "检测到系统: $OS"
    
    # 检查缺失的头文件
    echo "检查OpenGL头文件..."
    
    if [ ! -f "/usr/include/GL/gl.h" ] && [ ! -f "/usr/local/include/GL/gl.h" ]; then
        echo "警告: GL/gl.h 头文件未找到"
        
        if [ "$NO_DEPS" != "1" ]; then
            echo "尝试安装OpenGL开发库..."
            
            case $ID in
                ubuntu|debian|linuxmint)
                    echo "使用 apt 安装依赖..."
                    sudo apt update
                    sudo apt install -y libgl1-mesa-dev libglu1-mesa-dev mesa-common-dev
                    ;;
                fedora|centos|rhel)
                    echo "使用 dnf 或 yum 安装依赖..."
                    if command -v dnf &> /dev/null; then
                        sudo dnf install -y mesa-libGL-devel mesa-libGLU-devel
                    elif command -v yum &> /dev/null; then
                        sudo yum install -y mesa-libGL-devel mesa-libGLU-devel
                    fi
                    ;;
                arch|manjaro)
                    echo "使用 pacman 安装依赖..."
                    sudo pacman -Syu --noconfirm mesa glu
                    ;;
                *)
                    echo "无法自动安装依赖。请手动安装OpenGL开发库："
                    echo "对于Ubuntu/Debian: sudo apt install libgl1-mesa-dev libglu1-mesa-dev mesa-common-dev"
                    echo "对于Fedora/CentOS: sudo dnf install mesa-libGL-devel mesa-libGLU-devel"
                    echo "对于Arch: sudo pacman -S mesa glu"
                    echo "或使用 --no-deps 跳过依赖检查"
                    exit 1
                    ;;
            esac
            
            # 再次检查
            if [ ! -f "/usr/include/GL/gl.h" ] && [ ! -f "/usr/local/include/GL/gl.h" ]; then
                echo "警告: 安装后仍然找不到 GL/gl.h"
                echo "尝试创建符号链接..."
                
                # 尝试查找其他位置的gl.h
                find_path=$(find /usr -name "gl.h" 2>/dev/null | head -1)
                if [ -n "$find_path" ]; then
                    echo "找到 gl.h 在: $find_path"
                    sudo mkdir -p /usr/include/GL
                    sudo ln -sf "$find_path" /usr/include/GL/gl.h
                else
                    echo "错误: 无法找到 gl.h 文件"
                    exit 1
                fi
            fi
        else
            echo "跳过依赖安装 (使用 --no-deps 参数)"
        fi
    else
        echo "✓ OpenGL头文件已安装"
    fi
    
    # 检查其他可能的依赖
    echo "检查其他依赖..."
    
    # 检查GLFW头文件
    if [ ! -f "$SCRIPT_DIR/external/glfw/GLFW/glfw3.h" ]; then
        echo "警告: glfw3.h 在外部依赖中未找到"
        
        # 检查系统是否安装了GLFW
        if [ ! -f "/usr/include/GLFW/glfw3.h" ] && [ ! -f "/usr/local/include/GLFW/glfw3.h" ]; then
            echo "尝试安装GLFW开发库..."
            
            case $ID in
                ubuntu|debian|linuxmint)
                    sudo apt install -y libglfw3-dev
                    ;;
                fedora|centos|rhel)
                    if command -v dnf &> /dev/null; then
                        sudo dnf install -y glfw-devel
                    elif command -v yum &> /dev/null; then
                        sudo yum install -y glfw-devel
                    fi
                    ;;
                arch|manjaro)
                    sudo pacman -S --noconfirm glfw-x11
                    ;;
            esac
        fi
    fi
    
    # 检查pkg-config
    if ! command -v pkg-config &> /dev/null; then
        echo "安装 pkg-config..."
        case $ID in
            ubuntu|debian|linuxmint)
                sudo apt install -y pkg-config
                ;;
            fedora|centos|rhel)
                if command -v dnf &> /dev/null; then
                    sudo dnf install -y pkgconf-pkg-config
                elif command -v yum &> /dev/null; then
                    sudo yum install -y pkgconfig
                fi
                ;;
            arch|manjaro)
                sudo pacman -S --noconfirm pkg-config
                ;;
        esac
    fi
}

# 如果不跳过依赖检查
if [ "$NO_DEPS" != "1" ]; then
    check_and_install_deps
fi

# 检查并删除现有的 build 目录
if [ -d "build" ]; then
    echo "删除现有的 build 目录..."
    rm -rf build
fi

# 创建 build 目录
echo "创建 build 目录..."
mkdir -p build

# 进入 build 目录并执行 CMake
cd build || exit 1
echo "执行 CMake ($BUILD_TYPE)..."

# 检查OpenGL库是否可用
echo "检查OpenGL库..."
pkg-config --exists gl || echo "警告: OpenGL库可能不可用"

# 检测是否使用 Ninja 构建系统
if command -v ninja &> /dev/null; then
    echo "检测到 Ninja，使用 Ninja 构建系统"
    cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -GNinja ..
    BUILD_CMD="ninja"
else
    cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE ..
    BUILD_CMD="make -j$MAKE_JOBS"
fi

if [ $? -ne 0 ]; then
    echo "CMake 配置失败!"
    
    # 尝试使用不同的CMake参数
    echo "尝试备用CMake配置..."
    
    # 清理并重试
    rm -rf ./*
    
    # 尝试不同的配置
    cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
          -DCMAKE_CXX_FLAGS="-I/usr/include -I/usr/local/include" ..
    
    if [ $? -ne 0 ]; then
        echo "备用CMake配置也失败!"
        echo "请检查:"
        echo "1. CMakeLists.txt 文件"
        echo "2. 系统依赖是否已安装"
        echo "3. 运行: sudo apt install libgl1-mesa-dev libglu1-mesa-dev mesa-common-dev"
        exit 1
    fi
fi

# 执行构建
echo "执行构建 (使用 $MAKE_JOBS 个作业)..."
$BUILD_CMD

if [ $? -ne 0 ]; then
    echo "编译失败!"
    
    # 尝试使用更少的作业重试
    echo "使用单线程重试编译..."
    if [ "$BUILD_CMD" = "ninja" ]; then
        ninja
    else
        make
    fi
    
    if [ $? -ne 0 ]; then
        echo "编译仍然失败，请检查错误信息"
        exit 1
    fi
fi

echo "========================================"
echo "       项目初始化完成!"
echo "========================================"

# 显示构建结果
echo "构建结果:"
find . -name "*.so" -o -name "*.a" -o -name "*.exe" -o -name "StarryEngine" -type f 2>/dev/null | head -10

cd ..