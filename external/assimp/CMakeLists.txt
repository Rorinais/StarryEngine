get_filename_component(ASSIMP_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assimp/lib" ABSOLUTE)
# ★头文件用系统版 assimp(5.4.3):vendored 头的 aiQuatKey 是 24 字节旧布局,
# 缺 mInterpolation 字段;而链接/运行时加载的库(无论 vendored 5.3.0 还是系统
# 5.4.3)都以 32 字节布局写 mRotationKeys[]。头库不匹配 → 按 24 字节步长读数组
# → 索引≥1 的旋转键全错位,表现为 norm 2~6 的"坏键"/动画抽搐/时长被误截断。
# 改用系统头后布局与库一致,动画读取恢复正常(原始文件 42.5s 全有效键)。
set(ASSIMP_INC_DIR "/usr/include")

add_library(assimp SHARED IMPORTED GLOBAL)  


if(WIN32)
    if(MSVC)
        set_target_properties(assimp PROPERTIES
            IMPORTED_IMPLIB "${ASSIMP_LIB_DIR}/assimp-vc143-mt.lib"   
            IMPORTED_LOCATION "${ASSIMP_LIB_DIR}/assimp-vc143-mt.dll"
            INTERFACE_INCLUDE_DIRECTORIES "${ASSIMP_INC_DIR}"
        )
    elseif(MINGW)
        set_target_properties(assimp PROPERTIES
            IMPORTED_IMPLIB "${ASSIMP_LIB_DIR}/libassimp.dll.a"   
            IMPORTED_LOCATION "${ASSIMP_LIB_DIR}/libassimp-6.dll"
            INTERFACE_INCLUDE_DIRECTORIES "${ASSIMP_INC_DIR}"
        )
    endif()
elseif(UNIX AND NOT APPLE)
    set_target_properties(assimp PROPERTIES
        #IMPORTED_IMPLIB "${ASSIMP_LIB_DIR}/libunit.dll.a"   
        IMPORTED_LOCATION "${ASSIMP_LIB_DIR}/libassimp.so.5.3.0"
        INTERFACE_INCLUDE_DIRECTORIES "${ASSIMP_INC_DIR}"
    )
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(PUGIXML REQUIRED pugixml)
set_target_properties(assimp PROPERTIES
    INTERFACE_LINK_LIBRARIES "${PUGIXML_LIBRARIES}"
)
