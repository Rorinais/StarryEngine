@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

echo ========================================
echo       项目初始化构建脚本
echo ========================================

REM 获取脚本所在目录
set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"

REM 设置默认构建类型
set BUILD_TYPE=Debug
set MAKE_JOBS=4

REM 解析命令行参数
:parse_args
if "%~1"=="" goto end_args
if /i "%~1"=="debug" set BUILD_TYPE=Debug
if /i "%~1"=="release" set BUILD_TYPE=Release
if /i "%~1"=="jobs" (
    shift
    set MAKE_JOBS=%~1
)
shift
goto parse_args
:end_args

echo 构建类型: %BUILD_TYPE%
echo 并行编译数: %MAKE_JOBS%

REM 检查并删除现有的 build 目录
if exist "build" (
    echo 删除现有的 build 目录...
    rmdir /s /q "build"
)

REM 创建 build 目录
echo 创建 build 目录...
mkdir build

REM 进入 build 目录并执行 CMake
cd build
echo 执行 CMake (%BUILD_TYPE%)...
cmake -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ..

if !errorlevel! neq 0 (
    echo CMake 配置失败!
    pause
    exit /b !errorlevel!
)

REM 执行 Make
echo 执行 Make (使用 %MAKE_JOBS% 个作业)...
make -j%MAKE_JOBS%

if !errorlevel! neq 0 (
    echo Make 编译失败!
    pause
    exit /b !errorlevel!
)

echo ========================================
echo       项目初始化完成!
echo ========================================
cd ..
pause