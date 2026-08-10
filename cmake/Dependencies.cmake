if(NOT DEFINED STARRY_PREFER_VENDORED)
    set(STARRY_PREFER_VENDORED ON CACHE BOOL "优先使用 vendored 依赖（自包含/确定）；OFF 则系统 config 优先、vendored 兜底")
endif()

# find_package CONFIG 搜索 <prefix>/lib/cmake/<Pkg>*/，故伪包根目录 = external/<dep>
set(STARRY_VENDORED_ROOTS
    "${CMAKE_SOURCE_DIR}/external/vulkan"     # Vulkan / shaderc / spirv-cross-*
    "${CMAKE_SOURCE_DIR}/external/glfw"       # glfw3（target 名 glfw）
    "${CMAKE_SOURCE_DIR}/external/glm"        # glm
    "${CMAKE_SOURCE_DIR}/external/spdlog"     # spdlog
    "${CMAKE_SOURCE_DIR}/external/header_libs"# 单头库合集：stb_image / vk_mem_alloc / nlohmann_json
    "${CMAKE_SOURCE_DIR}/external/imgui"      # imgui（伪包内源码静态构建）
)
if(WIN32)
    list(APPEND STARRY_VENDORED_ROOTS "${CMAKE_SOURCE_DIR}/external/assimp")  # 仅 Windows
endif()

if(STARRY_PREFER_VENDORED)
    list(PREPEND CMAKE_PREFIX_PATH ${STARRY_VENDORED_ROOTS})
endif()

macro(starry_find_dep pkg vendored_root)
    if(STARRY_PREFER_VENDORED)
        find_package(${pkg} REQUIRED)
    else()
        find_package(${pkg} QUIET)
        if(NOT ${pkg}_FOUND)
            list(PREPEND CMAKE_PREFIX_PATH "${vendored_root}")
            find_package(${pkg} REQUIRED)
        endif()
    endif()
endmacro()

# ---------- Vulkan（特殊：先 CONFIG[vendored]，失败回退系统 MODULE） ----------
if(NOT STARRY_PREFER_VENDORED)
    find_package(Vulkan CONFIG QUIET)   
endif()
if(NOT TARGET Vulkan::Vulkan)
    find_package(Vulkan CONFIG QUIET)
endif()
if(NOT TARGET Vulkan::Vulkan)
    find_package(Vulkan QUIET)          
endif()
if(NOT TARGET Vulkan::Vulkan)
    if(NOT STARRY_PREFER_VENDORED)
        list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/external/vulkan")
        find_package(Vulkan CONFIG QUIET)
    endif()
endif()
if(NOT TARGET Vulkan::Vulkan)
    message(FATAL_ERROR "[deps] 未找到 Vulkan。装系统 Vulkan SDK/loader，或 -DSTARRY_PREFER_VENDORED=ON 用 vendored（默认已带 external/vulkan）。")
endif()

# ---------- glfw3 / glm / spdlog / imgui ----------
starry_find_dep(glfw3 "${CMAKE_SOURCE_DIR}/external/glfw")
starry_find_dep(glm "${CMAKE_SOURCE_DIR}/external/glm")
starry_find_dep(spdlog "${CMAKE_SOURCE_DIR}/external/spdlog")
starry_find_dep(imgui "${CMAKE_SOURCE_DIR}/external/imgui")

# ---------- assimp（Linux：vendored 源码静态构建；Windows：vendored 伪包 dll） ----------
if(NOT STARRY_PREFER_VENDORED)
    find_package(assimp QUIET)          
endif()
if(NOT TARGET assimp::assimp)
    if(WIN32)
        list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/external/assimp")
        find_package(assimp REQUIRED)  
    else()
        include(FetchContent)
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
        set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)      
        set(ASSIMP_BUILD_DRACO OFF CACHE BOOL "" FORCE)
        set(ASSIMP_IGNORE_GIT_HASH ON CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
        set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(assimp
            URL "file://${CMAKE_SOURCE_DIR}/external/assimp/assimp-5.4.3.tar.gz"
            URL_HASH "SHA256=a2350c1a20e27a14a5dffae78def7d111489f344805e950580f05130d30afc9f"
        )
        FetchContent_MakeAvailable(assimp)
        if(NOT TARGET assimp::assimp)
            add_library(assimp::assimp ALIAS assimp)   # 源码构建的 target 名是 assimp
        endif()

        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "15")
            target_compile_options(assimp PRIVATE -Wno-array-bounds)
        endif()
    endif()
endif()

# ---------- shaderc / spirv-cross（无系统包，固定 vendored） ----------
starry_find_dep(shaderc "${CMAKE_SOURCE_DIR}/external/vulkan")
starry_find_dep(spirv-cross-core "${CMAKE_SOURCE_DIR}/external/vulkan")
starry_find_dep(spirv-cross-glsl "${CMAKE_SOURCE_DIR}/external/vulkan")

# ---------- 单头库合集（header_libs：stb_image / vk_mem_alloc / nlohmann_json） ----------
starry_find_dep(vk_mem_alloc "${CMAKE_SOURCE_DIR}/external/header_libs")
starry_find_dep(stb_image "${CMAKE_SOURCE_DIR}/external/header_libs")
starry_find_dep(nlohmann_json "${CMAKE_SOURCE_DIR}/external/header_libs")
