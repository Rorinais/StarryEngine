@echo off
echo ========================================
echo       增量构建脚本（不删除目录）
echo ========================================

REM 设置默认构建类型
set BUILD_TYPE=Debug
if "%1"=="debug" set BUILD_TYPE=Debug
if "%1"=="release" set BUILD_TYPE=Release

echo 构建类型: %BUILD_TYPE%

REM 检查 build 目录是否存在
if not exist "build" (
    echo 错误: build 目录不存在
    echo 请先运行完整构建脚本或创建 build 目录
    pause
    exit /b 1
)

REM 进入 build 目录
cd build
echo 当前工作目录: %CD%

REM 执行 Make 进行增量构建
echo 执行增量构建...
make -j4

if errorlevel 1 (
    echo Make 编译失败!
    cd ..
    pause
    exit /b 1
)

echo ========================================
echo       增量构建成功!
echo ========================================
cd ..
pause