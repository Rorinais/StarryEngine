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

set(ICON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../assets/icons/app_icon.ico")

if(NOT EXISTS "${ICON_FILE}")
    message(WARNING "图标文件不存在: ${ICON_FILE}")
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/app_icon.ico "")
else()
    configure_file(${ICON_FILE} ${CMAKE_CURRENT_BINARY_DIR}/app_icon.ico COPYONLY)
endif()

set(RC_CONTENT "IDI_ICON1 ICON \"app_icon.ico\"\n")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/generated.rc "${RC_CONTENT}")

if(MINGW)
    set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/generated.rc PROPERTIES
        OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/app_icon.ico"
    )
endif()

set(EDITOR_OUTPUT_DIR ${BASE_OUTPUT_DIR}/${PROJECT_NAME})
set(VULKAN_MAIN_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/launch/vkTest1.cpp)

add_executable(${PROJECT_NAME} ${VULKAN_MAIN_SOURCE} ${CMAKE_CURRENT_BINARY_DIR}/generated.rc)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${EDITOR_OUTPUT_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    BaseInterface
    window
    application
    renderer
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


# 资源复制函数（增强版，使用BYPRODUCTS确保重新构建）
function(copy_target_resources TARGET_NAME RESOURCES_OUTPUT_DIR)
    cmake_parse_arguments(ARG "" "SHADERS;FONTS;MODELS;TEXTURES;ICONS" "" ${ARGN})
    
    # 获取目标的输出目录
    get_target_property(TARGET_OUTPUT_DIR ${TARGET_NAME} RUNTIME_OUTPUT_DIRECTORY)
    set(ASSETS_DEST "${TARGET_OUTPUT_DIR}/assets")

    # 确保资源目录存在
    file(MAKE_DIRECTORY "${ASSETS_DEST}")

    # 收集所有需要复制的文件
    set(ALL_RESOURCE_FILES "")
    set(ALL_COPY_COMMANDS "")
    
    # 处理Shader资源
    if(ARG_SHADERS)
        file(GLOB_RECURSE SHADER_FILES CONFIGURE_DEPENDS
            "${ARG_SHADERS}/*.vert"
            "${ARG_SHADERS}/*.frag"
            "${ARG_SHADERS}/*.comp"
            "${ARG_SHADERS}/*.glsl"
            "${ARG_SHADERS}/*.geom"
            "${ARG_SHADERS}/*.tesc"
            "${ARG_SHADERS}/*.tese"
        )
        
        foreach(shader_file IN LISTS SHADER_FILES)
            file(RELATIVE_PATH relative_path "${ARG_SHADERS}" "${shader_file}")
            set(final_dest "${ASSETS_DEST}/shaders/${relative_path}")
            get_filename_component(final_dir "${final_dest}" DIRECTORY)
            
            # 创建目录（如果不存在）
            list(APPEND ALL_COPY_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E make_directory "${final_dir}"
            )
            
            # 复制文件
            list(APPEND ALL_COPY_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${shader_file}"
                    "${final_dest}"
            )
            
            list(APPEND ALL_RESOURCE_FILES "${final_dest}")
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
            
            # 创建目录（如果不存在）
            list(APPEND ALL_COPY_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E make_directory "${final_dir}"
            )
            
            # 复制文件
            list(APPEND ALL_COPY_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${icon_file}"
                    "${final_dest}"
            )
            
            list(APPEND ALL_RESOURCE_FILES "${final_dest}")
        endforeach()
    endif()

    # 处理字体资源
    if(ARG_FONTS)
        file(GLOB_RECURSE FONT_FILES CONFIGURE_DEPENDS
            "${ARG_FONTS}/*.ttf"
            "${ARG_FONTS}/*.otf"
            "${ARG_FONTS}/*.fon"
            "${ARG_FONTS}/*.fnt"
        )
        
        foreach(font_file IN LISTS FONT_FILES)
            file(RELATIVE_PATH relative_path "${ARG_FONTS}" "${font_file}")
            set(final_dest "${ASSETS_DEST}/fonts/${relative_path}")
            get_filename_component(final_dir "${final_dest}" DIRECTORY)
            
            # 创建目录（如果不存在）
            list(APPEND ALL_COPY_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E make_directory "${final_dir}"
            )
            
            # 复制文件
            list(APPEND ALL_COPY_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${font_file}"
                    "${final_dest}"
            )
            
            list(APPEND ALL_RESOURCE_FILES "${final_dest}")
        endforeach()
    endif()

    # 处理模型资源
    if(ARG_MODELS)
        file(GLOB_RECURSE MODEL_FILES CONFIGURE_DEPENDS
            "${ARG_MODELS}/*.obj"
            "${ARG_MODELS}/*.fbx"
            "${ARG_MODELS}/*.dae"
            "${ARG_MODELS}/*.gltf"
            "${ARG_MODELS}/*.glb"
        )
        
        foreach(model_file IN LISTS MODEL_FILES)
            file(RELATIVE_PATH relative_path "${ARG_MODELS}" "${model_file}")
            set(final_dest "${ASSETS_DEST}/models/${relative_path}")
            get_filename_component(final_dir "${final_dest}" DIRECTORY)
            
            # 创建目录（如果不存在）
            list(APPEND ALL_COPY_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E make_directory "${final_dir}"
            )
            
            # 复制文件
            list(APPEND ALL_COPY_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${model_file}"
                    "${final_dest}"
            )
            
            list(APPEND ALL_RESOURCE_FILES "${final_dest}")
        endforeach()
    endif()

    # 处理纹理资源
    if(ARG_TEXTURES)
        file(GLOB_RECURSE TEXTURE_FILES CONFIGURE_DEPENDS
            "${ARG_TEXTURES}/*.png"
            "${ARG_TEXTURES}/*.jpg"
            "${ARG_TEXTURES}/*.jpeg"
            "${ARG_TEXTURES}/*.bmp"
            "${ARG_TEXTURES}/*.tga"
            "${ARG_TEXTURES}/*.tiff"
            "${ARG_TEXTURES}/*.gif"
            "${ARG_TEXTURES}/*.hdr"
            "${ARG_TEXTURES}/*.exr"
        )
        
        foreach(texture_file IN LISTS TEXTURE_FILES)
            file(RELATIVE_PATH relative_path "${ARG_TEXTURES}" "${texture_file}")
            set(final_dest "${ASSETS_DEST}/textures/${relative_path}")
            get_filename_component(final_dir "${final_dest}" DIRECTORY)
            
            # 创建目录（如果不存在）
            list(APPEND ALL_COPY_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E make_directory "${final_dir}"
            )
            
            # 复制文件
            list(APPEND ALL_COPY_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${texture_file}"
                    "${final_dest}"
            )
            
            list(APPEND ALL_RESOURCE_FILES "${final_dest}")
        endforeach()
    endif()

    # 如果没有需要复制的资源，则直接返回
    list(LENGTH ALL_COPY_COMMANDS NUM_COMMANDS)
    if(NUM_COMMANDS EQUAL 0)
        return()
    endif()

    # 创建一个自定义命令来复制所有资源
    add_custom_command(
        OUTPUT ${ALL_RESOURCE_FILES}
        COMMAND ${CMAKE_COMMAND} -E echo "复制资源文件到 ${ASSETS_DEST}/"
        ${ALL_COPY_COMMANDS}
        COMMENT "复制资源文件到目标目录"
        DEPENDS ${SHADER_FILES} ${ICON_FILES} ${FONT_FILES} ${MODEL_FILES} ${TEXTURE_FILES}
        VERBATIM
    )
    
    # 创建一个自定义目标来触发资源复制
    add_custom_target(${TARGET_NAME}_copy_resources ALL
        DEPENDS ${ALL_RESOURCE_FILES}
    )
    
    # 让主目标依赖于资源复制目标
    add_dependencies(${TARGET_NAME} ${TARGET_NAME}_copy_resources)
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