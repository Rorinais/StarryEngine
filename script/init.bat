@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion
rem ========================================
rem   StarryEngine 全量清理构建脚本 (Windows)
rem ========================================
rem  用法:
rem     init.bat [release|debug] [-j N] [-d DIR] [-t TARGET]
rem
rem  项目零系统依赖（vendored 伪包 + assimp 源码构建），本脚本不做任何系统包
rem  安装。唯一系统要求: 可用的 GPU Vulkan 驱动（NVIDIA / Mesa 开源驱动）。
rem  默认 Release 构建；Debug 会定义 ENABLE_VALIDATION_LAYERS 并复制验证层，
rem  需要验证层与驱动可用，一般开发用 Release 即可。产物在 <构建目录>/bin/。
rem
rem  生成器为 Visual Studio 时用 --config 选配置；Ninja/MinGW 单配置生成器则靠
rem  configure 时的 -DCMAKE_BUILD_TYPE。两处都传，两种情况都正确。

set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
set "ROOT_DIR=%SCRIPT_DIR%\.."
cd /d "%ROOT_DIR%"

set "BUILD_TYPE=Release"
set "BUILD_DIR="
set "JOBS=%NUMBER_OF_PROCESSORS%"
set "TARGET="

call :parse_args %*
if errorlevel 1 exit /b 1

rem 未指定构建目录时，按构建类型派生（build\release 或 build\debug）
if "%BUILD_DIR%"=="" (
    if /i "%BUILD_TYPE%"=="Debug" (
        set "BUILD_DIR=build\debug"
    ) else (
        set "BUILD_DIR=build\release"
    )
)

echo 构建类型: %BUILD_TYPE%
echo 构建目录: %BUILD_DIR%
echo 并行数:   %JOBS%
if not "%TARGET%"=="" echo 目标:     %TARGET%

if /i "%BUILD_TYPE%"=="Debug" (
    echo 提示: Debug 会启用 Vulkan 验证层（ENABLE_VALIDATION_LAYERS），
    echo       若报验证层相关错误，请改用 Release。
)

rem 全新构建（删目录，保证干净）
if exist "%BUILD_DIR%" (
    echo 删除旧的构建目录 %BUILD_DIR% ...
    rmdir /s /q "%BUILD_DIR%"
    if errorlevel 1 (
        echo 错误: 无法删除 %BUILD_DIR%（可能被占用），请手动删除后重试。
        exit /b 1
    )
)

echo ==^> CMake 配置...
cmake -S . -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
if errorlevel 1 (
    echo CMake 配置失败!
    exit /b 1
)

echo ==^> 全量构建...
if not "%TARGET%"=="" (
    cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% --target %TARGET% --parallel %JOBS%
) else (
    cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% --parallel %JOBS%
)
if errorlevel 1 (
    echo 构建失败!
    exit /b 1
)

echo ========================================
echo        全量构建成功!
echo ========================================
echo 产物: %BUILD_DIR%\<demo>\（每个项目独立目录，exe+assets 自包含；release/debug 分目录）
echo 运行: run.bat           （自动增量构建 + 运行，默认 DeferredRender_demo）
echo   或: run.bat -t test1  （指定 demo）
exit /b 0

rem ---------- 参数解析 ----------
:parse_args
:parse_loop
if "%~1"=="" goto :eof
if /i "%~1"=="release" (
    set "BUILD_TYPE=Release"
) else if /i "%~1"=="-release" (
    set "BUILD_TYPE=Release"
) else if /i "%~1"=="debug" (
    set "BUILD_TYPE=Debug"
) else if /i "%~1"=="-debug" (
    set "BUILD_TYPE=Debug"
) else if "%~1"=="-j" (
    if "%~2"=="" ( echo -j 需要参数 & call :usage & exit /b 1 )
    set "JOBS=%~2"
    shift
) else if "%~1"=="-d" (
    if "%~2"=="" ( echo -d 需要参数 & call :usage & exit /b 1 )
    set "BUILD_DIR=%~2"
    shift
) else if "%~1"=="-t" (
    if "%~2"=="" ( echo -t 需要参数 & call :usage & exit /b 1 )
    set "TARGET=%~2"
    shift
) else if "%~1"=="-h" (
    call :usage
    exit /b 1
) else (
    echo 未知参数: %~1
    call :usage
    exit /b 1
)
shift
goto :parse_loop

:usage
echo 用法: %~nx0 [release^|debug] [-j N] [-d DIR] [-t TARGET]
echo   release^|debug  构建类型（默认 release）
echo   -j N           并行数（默认 = CPU 核数）
echo   -d DIR         构建目录（默认 build\release 或 build\debug，按类型）
echo   -t TARGET      只构建指定目标（demo 名）；省略则构建全部
echo                  产物进 TARGET\（单一构建树，静态库只编一次）
exit /b 0
