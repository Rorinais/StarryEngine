# 创建基础接口库
add_library(BaseInterface INTERFACE)
target_include_directories(BaseInterface INTERFACE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/external
)

target_link_libraries(BaseInterface INTERFACE
    assimp
    glfw
    glm
    imgui
    spdlog
    vulkan 
    shaderc
)

add_subdirectory(core/application)
add_subdirectory(core/platform)
add_subdirectory(renderer)


# 设置图标文件路径
set(ICON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../assets/icons/app_icon.ico")

# 检查图标文件是否存在
if(NOT EXISTS "${ICON_FILE}")
    message(WARNING "图标文件不存在: ${ICON_FILE}")
    # 创建一个空文件或跳过图标设置
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/app_icon.ico "")
else()
    # 复制图标文件到构建目录
    configure_file(${ICON_FILE} ${CMAKE_CURRENT_BINARY_DIR}/app_icon.ico COPYONLY)
endif()

# 生成资源文件
set(RC_CONTENT "IDI_ICON1 ICON \"app_icon.ico\"\n")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/generated.rc "${RC_CONTENT}")

# 对于MinGW，设置资源文件依赖
if(MINGW)
    set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/generated.rc PROPERTIES
        OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/app_icon.ico"
    )
endif()

# 创建主编辑器可执行文件
set(EDITOR_OUTPUT_DIR ${BASE_OUTPUT_DIR}/${PROJECT_NAME})
set(VULKAN_MAIN_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/launch/vkTest1.cpp)

add_executable(${PROJECT_NAME} ${VULKAN_MAIN_SOURCE} ${CMAKE_CURRENT_BINARY_DIR}/generated.rc)
# 设置输出目录
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${EDITOR_OUTPUT_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    BaseInterface
    window
    application
    core
    pipeline
    resources
)

# 创建OpenCV调试可执行文件
#set(OPENCV_OUTPUT_DIR ${BASE_OUTPUT_DIR}/OpenCV_Debug)
#set(OPENCV_MAIN_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/launch/opencv.cpp)

#add_executable(opencv_debug ${OPENCV_MAIN_SOURCE})
# 设置输出目录
#set_target_properties(opencv_debug PROPERTIES
#    RUNTIME_OUTPUT_DIRECTORY ${OPENCV_OUTPUT_DIR}
#)

#target_link_libraries(opencv_debug PRIVATE 
#    opencv
#)


# 资源复制函数（更新版本，包含ICONS处理）
function(copy_target_resources TARGET_NAME RESOURCES_OUTPUT_DIR)
    cmake_parse_arguments(ARG "" "SHADERS;FONTS;MODELS;TEXTURES;ICONS" "" ${ARGN})
    
    # 获取目标的输出目录
    get_target_property(TARGET_OUTPUT_DIR ${TARGET_NAME} RUNTIME_OUTPUT_DIRECTORY)
    set(ASSETS_DEST "${TARGET_OUTPUT_DIR}/assets")

    # 确保资源目录存在
    file(MAKE_DIRECTORY "${ASSETS_DEST}")

    # 处理Shader资源
    if(ARG_SHADERS)
        file(GLOB_RECURSE SHADER_FILES CONFIGURE_DEPENDS
            "${ARG_SHADERS}/*.vert"
            "${ARG_SHADERS}/*.frag"
            "${ARG_SHADERS}/*.comp"
            "${ARG_SHADERS}/*.glsl"
        )
        
        foreach(shader_file IN LISTS SHADER_FILES)
            file(RELATIVE_PATH relative_path "${ARG_SHADERS}" "${shader_file}")
            set(final_dest "${ASSETS_DEST}/shaders/${relative_path}")
            get_filename_component(final_dir "${final_dest}" DIRECTORY)
            
            add_custom_command(TARGET ${TARGET_NAME} PRE_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "${final_dir}"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${shader_file}"
                    "${final_dest}"
                COMMENT "复制Shader: ${relative_path}"
            )
        endforeach()
    endif()

    # 处理图标资源
    if(ARG_ICONS)
        file(GLOB_RECURSE ICON_FILES CONFIGURE_DEPENDS
            "${ARG_ICONS}/*.ico"
            "${ARG_ICONS}/*.png"
            "${ARG_ICONS}/*.jpg"
            "${ARG_ICONS}/*.jpeg"
        )
        
        foreach(icon_file IN LISTS ICON_FILES)
            file(RELATIVE_PATH relative_path "${ARG_ICONS}" "${icon_file}")
            set(final_dest "${ASSETS_DEST}/icons/${relative_path}")
            get_filename_component(final_dir "${final_dest}" DIRECTORY)
            
            add_custom_command(TARGET ${TARGET_NAME} PRE_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "${final_dir}"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${icon_file}"
                    "${final_dest}"
                COMMENT "复制图标: ${relative_path}"
            )
        endforeach()
    endif()

    # 处理其他资源
    foreach(res_type FONTS MODELS TEXTURES)
        if(ARG_${res_type})
            string(TOLOWER "${res_type}" res_dir)
            file(GLOB_RECURSE RES_FILES CONFIGURE_DEPENDS "${ARG_${res_type}}/*")
            
            foreach(res_file IN LISTS RES_FILES)
                file(RELATIVE_PATH relative_path "${ARG_${res_type}}" "${res_file}")
                set(final_dest "${ASSETS_DEST}/${res_dir}/${relative_path}")
                get_filename_component(final_dir "${final_dest}" DIRECTORY)
                
                add_custom_command(TARGET ${TARGET_NAME} PRE_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${final_dir}"
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${res_file}"
                        "${final_dest}"
                    COMMENT "复制${res_type}: ${relative_path}"
                )
            endforeach()
        endif()
    endforeach()
endfunction()

# 为编辑器复制资源
copy_target_resources(${PROJECT_NAME} ${EDITOR_OUTPUT_DIR}
    SHADERS    ${SHADERS_DIR}
    FONTS      ${FONTS_DIR}
    MODELS     ${MODELS_DIR}
    TEXTURES   ${TEXTURES_DIR}
    ICONS      ${ICONS_DIR}
)

# 通用DLL复制函数
function(copy_target_dll target dll_path)
    get_target_property(TARGET_OUTPUT_DIR ${target} RUNTIME_OUTPUT_DIRECTORY)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${dll_path}"
            "${TARGET_OUTPUT_DIR}"
        COMMENT "复制依赖DLL到 ${target} 的输出目录"
    )
endfunction()

# 为编辑器复制依赖DLL
if(WIN32)
    if(MSVC)
        copy_target_dll(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/external/glfw/glfw3.dll")
        copy_target_dll(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/external/vulkan/lib/vulkan-1.dll")
        copy_target_dll(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/external/assimp/assimp/lib/assimp-vc143-mt.dll")
        copy_target_dll(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/external/vulkan/lib/shaderc/shaderc_sharedd.dll")
    elseif(MINGW)
        copy_target_dll(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/external/glfw/libglfw3.a")
        copy_target_dll(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/external/vulkan/lib/vulkan-1.dll")
        copy_target_dll(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/external/assimp/assimp/lib/assimp-vc143-mt.dll")
        copy_target_dll(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/external/vulkan/lib/shaderc/shaderc_sharedd.dll")
    endif()
endif()

# 为OpenCV调试程序复制依赖DLL
#if(CMAKE_BUILD_TYPE STREQUAL "Debug")
#    if(WIN32)
#        if(MSVC)
#            copy_target_dll(opencv_debug "${CMAKE_SOURCE_DIR}/external/opencv/vc16/bin/opencv_world4110d.dll")
#            copy_target_dll(opencv_debug "${CMAKE_SOURCE_DIR}/external/opencv/vc16/bin/opencv_videoio_msmf4110_64d.dll")
#        endif()
#    endif()
#elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
#    if(WIN32)
#        if(MSVC)
#            copy_target_dll(opencv_debug "${CMAKE_SOURCE_DIR}/external/opencv/vc16/bin/opencv_world4110.dll")
#            copy_target_dll(opencv_debug "${CMAKE_SOURCE_DIR}/external/opencv/vc16/bin/opencv_videoio_msmf4110_64.dll")
#        endif()
#    endif()
#endif()



# Vulkan验证层配置
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        ENABLE_VALIDATION_LAYERS
    )

    # 验证层文件处理
    set(VK_LAYER_SRC "${CMAKE_SOURCE_DIR}/external/vulkan/lib/layers")
    set(VK_LAYER_DEST "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
    
    if(WIN32)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${VK_LAYER_DEST}/layers"        
            COMMAND ${CMAKE_COMMAND} -E copy
                "${VK_LAYER_SRC}/VkLayer_khronos_validation.dll"
                "${VK_LAYER_DEST}/"
            COMMAND ${CMAKE_COMMAND} -E copy
                "${VK_LAYER_SRC}/VkLayer_khronos_validation.json"
                "${VK_LAYER_DEST}/layers"
            COMMENT "Copying Vulkan validation layers"
        )

        set_target_properties(${PROJECT_NAME} PROPERTIES
            VS_DEBUGGER_ENVIRONMENT 
            "VK_LAYER_PATH=${VK_LAYER_DEST};%VK_LAYER_PATH%"
        )
        
    endif()
    
endif()
