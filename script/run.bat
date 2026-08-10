@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion
rem ========================================
rem   StarryEngine 智能运行脚本 (Windows)
rem ========================================
rem  用法:
rem     run.bat [release|debug] [-f] [-d DIR] [-t TARGET] [-- 程序参数...]
rem
rem  自动判断: 可执行文件缺失 / --force / 源码或资源更新 → 先增量构建再运行。
rem  demo 以相对路径加载资源（bin/assets/），故在 bin/ 目录下执行。默认 Release。
rem  -t 指定 demo 目标（默认 DeferredRender_demo）。

set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
set "ROOT_DIR=%SCRIPT_DIR%\.."
cd /d "%ROOT_DIR%"

set "BUILD_TYPE=Release"
set "BUILD_DIR="
set "TARGET=DeferredRender_demo"
set "FORCE=0"

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

rem 单一构建树 + 每项目独立目录（与项目同名）：exe 在 <build>\<TARGET>\<TARGET>.exe
set "EXECUTABLE_PATH=%ROOT_DIR%\%BUILD_DIR%\%TARGET%\%TARGET%.exe"

rem ---- 判断是否需要构建 ----
set "NEED_BUILD=0"
if "%FORCE%"=="1" set "NEED_BUILD=1"
if not exist "%EXECUTABLE_PATH%" set "NEED_BUILD=1"

if "%NEED_BUILD%"=="0" (
    rem 最后构建时间（取自可执行文件）
    set "LAST_BUILD_TIME="
    for %%F in ("%EXECUTABLE_PATH%") do set "LAST_BUILD_TIME=%%~tF"

    rem 源码比可执行文件新 → 重建
    for /r "%ROOT_DIR%\src" %%F in (*.cpp *.hpp *.h *.c *.cxx *.cc) do (
        for %%G in ("%%F") do (
            if "%%~tG" gtr "!LAST_BUILD_TIME!" (
                echo 检测到源码更改: %%~nxF
                set "NEED_BUILD=1"
                goto :check_build
            )
        )
    )

    rem 资源比可执行文件新 → 重建
    if exist "%ROOT_DIR%\assets" (
        for /r "%ROOT_DIR%\assets" %%F in (
            *.glsl *.vert *.frag *.comp *.geom *.tesc *.tese
            *.png *.jpg *.jpeg *.bmp *.tga *.hdr *.ico *.ttf *.otf
            *.obj *.fbx *.dae *.gltf *.glb *.mtl
            *.json *.yml *.yaml *.ini *.cfg
        ) do (
            for %%G in ("%%F") do (
                if "%%~tG" gtr "!LAST_BUILD_TIME!" (
                    echo 检测到资源更改: %%~nxF
                    set "NEED_BUILD=1"
                    goto :check_build
                )
            )
        )
    )
)

:check_build
if "%NEED_BUILD%"=="1" (
    echo ==^> 需要构建，调用 build.bat ...
    if exist "%ROOT_DIR%\%BUILD_DIR%" (
        call "%SCRIPT_DIR%\build.bat" %BUILD_TYPE% -d %BUILD_DIR% -t %TARGET%
    ) else (
        echo ==^> 首次构建（目录不存在）...
        call "%SCRIPT_DIR%\init.bat" %BUILD_TYPE% -d %BUILD_DIR% -t %TARGET%
    )
    if errorlevel 1 (
        echo 构建失败，无法运行程序!
        exit /b 1
    )
) else (
    echo 没有检测到更改，使用现有构建...
)

if not exist "%EXECUTABLE_PATH%" (
    echo 错误: 可执行文件不存在: %EXECUTABLE_PATH%
    exit /b 1
)

rem ---- 运行（在 bin/ 目录下，资源相对路径加载）----
echo 正在运行: %BUILD_DIR%\%TARGET%\%TARGET%.exe
cd /d "%ROOT_DIR%\%BUILD_DIR%\bin"
"%TARGET%.exe"
set "EXIT_CODE=%ERRORLEVEL%"
echo.
echo 程序退出，代码: %EXIT_CODE%
cd /d "%ROOT_DIR%"
exit /b %EXIT_CODE%

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
) else if "%~1"=="-f" (
    set "FORCE=1"
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
echo 用法: %~nx0 [release^|debug] [-f] [-d DIR] [-t TARGET]
echo   release^|debug  构建类型（默认 release）
echo   -f, --force     强制重新构建（忽略时间戳）
echo   -d DIR          构建目录（默认 build\release 或 build\debug，按类型）
echo   -t TARGET       demo 目标名（默认 DeferredRender_demo；可选 test1 /
echo                   TransparentWindow_demo / InferenceChain_demo）。
echo                   产物进 TARGET\（单一构建树，静态库只编一次）
exit /b 0
