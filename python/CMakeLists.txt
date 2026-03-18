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