@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

echo ========================================
echo       智能运行脚本
echo ========================================

REM 获取脚本所在目录
set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"

REM 设置默认构建类型
set BUILD_TYPE=Debug
set EXECUTABLE_NAME=StarryEngine
set EXECUTABLE_PATH=%SCRIPT_DIR%\build\bin\StarryEngine
set BUILD_NEEDED=0
set BUILD_CLEAN=0

REM 解析命令行参数
:parse_args
if "%~1"=="" goto end_args
if /i "%~1"=="debug" set BUILD_TYPE=Debug
if /i "%~1"=="release" set BUILD_TYPE=Release
if /i "%~1"=="exe" (
    shift
    set EXECUTABLE_NAME=%~1
)
if /i "%~1"=="path" (
    shift
    set EXECUTABLE_PATH=%~1
)
if /i "%~1"=="force" set BUILD_NEEDED=1
if /i "%~1"=="clean" set BUILD_CLEAN=1
shift
goto parse_args
:end_args

echo 运行类型: %BUILD_TYPE%
echo 可执行文件: %EXECUTABLE_NAME%
echo 路径: %EXECUTABLE_PATH%

REM 检查构建目录是否存在
if not exist "%SCRIPT_DIR%\build" (
    echo 构建目录不存在，需要重新构建...
    set BUILD_NEEDED=1
    goto check_build
)

REM 检查可执行文件是否存在
if not exist "%EXECUTABLE_PATH%\%EXECUTABLE_NAME%.exe" (
    echo 可执行文件不存在，需要重新构建...
    set BUILD_NEEDED=1
    goto check_build
)

REM 检查源代码修改时间
echo 检查源代码修改情况...
set LAST_BUILD_TIME=
for %%F in ("%EXECUTABLE_PATH%\%EXECUTABLE_NAME%.exe") do set LAST_BUILD_TIME=%%~tF

echo 最后构建时间: !LAST_BUILD_TIME!

REM 检查源代码目录中的文件是否比可执行文件新
for /r "%SCRIPT_DIR%\src" %%F in (*.cpp *.hpp *.h *.c) do (
    for %%G in ("%%F") do (
        if "%%~tG" gtr "!LAST_BUILD_TIME!" (
            echo 检测到源代码更改: %%~nxF
            set BUILD_NEEDED=1
            goto check_build
        )
    )
)

REM 检查资源文件是否更改
if exist "%SCRIPT_DIR%\assets" (
    for /r "%SCRIPT_DIR%\assets" %%F in (*.glsl *.vert *.frag *.comp *.png *.jpg *.jpeg *.ico) do (
        for %%G in ("%%F") do (
            if "%%~tG" gtr "!LAST_BUILD_TIME!" (
                echo 检测到资源文件更改: %%~nxF
                set BUILD_NEEDED=1
                goto check_build
            )
        )
    )
)

:check_build
if !BUILD_NEEDED! equ 1 (
    echo.
    echo 需要重新构建项目...
    
    REM 确定构建选项
    set BUILD_OPTION=
    if "!BUILD_CLEAN!"=="1" set BUILD_OPTION=clean
    
    call "%SCRIPT_DIR%\build.bat" %BUILD_TYPE% %BUILD_OPTION%
    
    if !errorlevel! neq 0 (
        echo 构建失败，无法运行程序!
        pause
        exit /b !errorlevel!
    )
) else (
    echo 没有检测到更改，使用现有构建...
)

REM 检查可执行文件是否存在
if not exist "%EXECUTABLE_PATH%\%EXECUTABLE_NAME%.exe" (
    echo 错误: 可执行文件不存在: %EXECUTABLE_PATH%\%EXECUTABLE_NAME%.exe
    echo.
    echo 请先运行构建脚本: build.bat %BUILD_TYPE%
    pause
    exit /b 1
)

REM 运行可执行文件
echo.
echo 运行: %EXECUTABLE_PATH%\%EXECUTABLE_NAME%.exe
cd /d "%EXECUTABLE_PATH%"
"%EXECUTABLE_NAME%.exe"
cd /d "%SCRIPT_DIR%"

echo ========================================
echo       程序执行完毕
echo ========================================
pause