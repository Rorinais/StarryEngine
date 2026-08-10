@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion
rem ========================================
rem   StarryEngine 增量构建脚本 (Windows)
rem ========================================
rem  用法:
rem     build.bat [release|debug] [-j N] [-d DIR] [-t TARGET]
rem
rem  每次先重跑 cmake configure（保持编译数据库 compile_commands.json 最新，
rem  IDE 智能提示依赖它），再做增量编译。默认 Release。产物在 <构建目录>/bin/。
rem  不指定 -t 时构建全部目标。

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

if not exist "%BUILD_DIR%" (
    echo 错误: 构建目录 %BUILD_DIR% 不存在
    echo 请先运行: init.bat [-d %BUILD_DIR%]
    exit /b 1
)

echo 构建类型: %BUILD_TYPE%
echo 构建目录: %BUILD_DIR%
echo 并行数:   %JOBS%
if not "%TARGET%"=="" echo 目标:     %TARGET%

rem 重跑 configure：既保证选项最新，也刷新 compile_commands.json（IDE 用）
echo ==^> 检查 CMake 配置...
cmake -S . -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
if errorlevel 1 (
    echo CMake 配置失败!
    exit /b 1
)

echo ==^> 增量构建...
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
echo        增量构建成功!
echo ========================================
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
echo   -t TARGET      只构建指定目标（demo 名，如 DeferredRender_demo / test1 /
echo                  TransparentWindow_demo / InferenceChain_demo）；省略则构建全部。
echo                  产物进 TARGET\（单一构建树，静态库只编一次）
exit /b 0
