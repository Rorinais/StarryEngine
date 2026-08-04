# ════════════════════════════════════════════════════════════
# 优先使用项目捆绑的便携 Python（external/python），编译不依赖系统环境；
# 找不到捆绑解释器时回退系统 Python。
# ════════════════════════════════════════════════════════════
set(STARRY_PYTHON_ROOT "${CMAKE_SOURCE_DIR}/external/python")
set(STARRY_PYTHON_BIN "${STARRY_PYTHON_ROOT}/bin/python3.13")
set(STARRY_PYTHON_INCLUDE "${STARRY_PYTHON_ROOT}/include/python3.13")

if(EXISTS "${STARRY_PYTHON_BIN}" AND EXISTS "${STARRY_PYTHON_INCLUDE}/Python.h")
    message(STATUS "[python] 使用捆绑 Python: ${STARRY_PYTHON_ROOT}")
    # 关闭 pybind11 的自动探测覆盖，手动指定捆绑路径
    set(PYBIND11_PYTHONLIBS_OVERWRITE OFF CACHE BOOL "Use bundled Python paths" FORCE)
    set(PYTHON_EXECUTABLE "${STARRY_PYTHON_BIN}" CACHE FILEPATH "Bundled Python" FORCE)
    set(PYTHON_INCLUDE_DIR "${STARRY_PYTHON_INCLUDE}" CACHE PATH "Bundled Python includes" FORCE)
    if(EXISTS "${STARRY_PYTHON_ROOT}/lib/libpython3.13.so")
        set(PYTHON_LIBRARY "${STARRY_PYTHON_ROOT}/lib/libpython3.13.so" CACHE FILEPATH "Bundled libpython" FORCE)
    elseif(EXISTS "${STARRY_PYTHON_ROOT}/lib/libpython3.13.a")
        set(PYTHON_LIBRARY "${STARRY_PYTHON_ROOT}/lib/libpython3.13.a" CACHE FILEPATH "Bundled libpython" FORCE)
    endif()
else()
    message(STATUS "[python] 未找到捆绑 Python，回退系统 Python（可手动指定 -DPYTHON_INCLUDE_DIR=...）")
endif()

pybind11_add_module(starryengine_py
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/StarryEngineBindings.cpp
)

target_link_libraries(starryengine_py PRIVATE
    application
    renderer
    assets
    scene
    utils
    core
    event
    logging
    BaseInterface
)

# Ubuntu 布局：pyconfig.h 位于多架构子目录（如 x86_64-linux-gnu/python3.13/）
# 把 PYTHON_INCLUDE_DIR 的父目录加入包含路径，使 <x86_64-linux-gnu/python3.13/pyconfig.h> 可被解析
get_filename_component(PY_PLAT_INCLUDE_ROOT "${PYTHON_INCLUDE_DIR}" DIRECTORY)
target_include_directories(starryengine_py PRIVATE "${PY_PLAT_INCLUDE_ROOT}")

set_target_properties(starryengine_py PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${BASE_OUTPUT_DIR}/python
    RUNTIME_OUTPUT_DIRECTORY ${BASE_OUTPUT_DIR}/python
)

if(WIN32 AND MSVC)
    copy_target_dll(starryengine_py "${CMAKE_SOURCE_DIR}/external/glfw/glfw3.dll")
    copy_target_dll(starryengine_py "${CMAKE_SOURCE_DIR}/external/vulkan/lib/vulkan-1.dll")
    copy_target_dll(starryengine_py "${CMAKE_SOURCE_DIR}/external/assimp/assimp/lib/assimp-vc143-mt.dll")
    copy_target_dll(starryengine_py "${CMAKE_SOURCE_DIR}/external/vulkan/lib/shaderc/shaderc_sharedd.dll")
endif()


if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(VK_LAYER_SRC "${CMAKE_SOURCE_DIR}/external/vulkan/lib/layers")
    get_target_property(PY_MODULE_OUTPUT_DIR starryengine_py LIBRARY_OUTPUT_DIRECTORY)
    if(NOT PY_MODULE_OUTPUT_DIR)
        set(PY_MODULE_OUTPUT_DIR ${BASE_OUTPUT_DIR}/python)
    endif()

    add_custom_command(TARGET starryengine_py POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PY_MODULE_OUTPUT_DIR}/layers"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VK_LAYER_SRC}/VkLayer_khronos_validation.dll"
            "${PY_MODULE_OUTPUT_DIR}/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VK_LAYER_SRC}/VkLayer_khronos_validation.json"
            "${PY_MODULE_OUTPUT_DIR}/layers"
        COMMENT "复制Vulkan验证层到Python模块目录"
    )
endif()