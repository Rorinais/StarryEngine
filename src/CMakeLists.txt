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
    spirv-cross-glsl
    spirv-cross-core
)

add_subdirectory(core)
add_subdirectory(assets)
add_subdirectory(scene)
add_subdirectory(event)
add_subdirectory(utils)
add_subdirectory(logging)
add_subdirectory(application)
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
set(VULKAN_MAIN_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/application/Application.cpp)

add_executable(${PROJECT_NAME} ${VULKAN_MAIN_SOURCE} ${CMAKE_CURRENT_BINARY_DIR}/generated.rc)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${EDITOR_OUTPUT_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    BaseInterface
    utils
    core
    application
    renderer
    assets
    scene
)

function(copy_target_resources TARGET_NAME RESOURCES_OUTPUT_DIR)
    cmake_parse_arguments(ARG "" "SHADERS;FONTS;MODELS;TEXTURES;ICONS" "" ${ARGN})

    get_target_property(TARGET_OUTPUT_DIR ${TARGET_NAME} RUNTIME_OUTPUT_DIRECTORY)
    set(ASSETS_DEST "${TARGET_OUTPUT_DIR}/assets")

    file(MAKE_DIRECTORY "${ASSETS_DEST}")

    set(ALL_RESOURCE_FILES "")
    set(ALL_COPY_COMMANDS "")

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

            list(APPEND ALL_COPY_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E make_directory "${final_dir}"
            )

            list(APPEND ALL_COPY_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${shader_file}"
                    "${final_dest}"
            )
            
            list(APPEND ALL_RESOURCE_FILES "${final_dest}")
        endforeach()
    endif()

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

            list(APPEND ALL_COPY_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E make_directory "${final_dir}"
            )

            list(APPEND ALL_COPY_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${icon_file}"
                    "${final_dest}"
            )
            
            list(APPEND ALL_RESOURCE_FILES "${final_dest}")
        endforeach()
    endif()

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

            list(APPEND ALL_COPY_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E make_directory "${final_dir}"
            )

            list(APPEND ALL_COPY_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${font_file}"
                    "${final_dest}"
            )
            
            list(APPEND ALL_RESOURCE_FILES "${final_dest}")
        endforeach()
    endif()

    if(ARG_MODELS)
        file(GLOB_RECURSE MODEL_FILES CONFIGURE_DEPENDS
            "${ARG_MODELS}/*.obj"
            "${ARG_MODELS}/*.fbx"
            "${ARG_MODELS}/*.dae"
            "${ARG_MODELS}/*.gltf"
            "${ARG_MODELS}/*.glb"
            "${ARG_MODELS}/*.mtl"
        )
        
        foreach(model_file IN LISTS MODEL_FILES)
            file(RELATIVE_PATH relative_path "${ARG_MODELS}" "${model_file}")
            set(final_dest "${ASSETS_DEST}/models/${relative_path}")
            get_filename_component(final_dir "${final_dest}" DIRECTORY)

            list(APPEND ALL_COPY_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E make_directory "${final_dir}"
            )

            list(APPEND ALL_COPY_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${model_file}"
                    "${final_dest}"
            )
            
            list(APPEND ALL_RESOURCE_FILES "${final_dest}")
        endforeach()
    endif()

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

            list(APPEND ALL_COPY_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E make_directory "${final_dir}"
            )
            
            list(APPEND ALL_COPY_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${texture_file}"
                    "${final_dest}"
            )
            
            list(APPEND ALL_RESOURCE_FILES "${final_dest}")
        endforeach()
    endif()

    list(LENGTH ALL_COPY_COMMANDS NUM_COMMANDS)
    if(NUM_COMMANDS EQUAL 0)
        return()
    endif()

    add_custom_command(
        OUTPUT ${ALL_RESOURCE_FILES}
        COMMAND ${CMAKE_COMMAND} -E echo "复制资源文件到 ${ASSETS_DEST}/"
        ${ALL_COPY_COMMANDS}
        COMMENT "复制资源文件到目标目录"
        DEPENDS ${SHADER_FILES} ${ICON_FILES} ${FONT_FILES} ${MODEL_FILES} ${TEXTURE_FILES}
        VERBATIM
    )
    
    add_custom_target(${TARGET_NAME}_copy_resources ALL
        DEPENDS ${ALL_RESOURCE_FILES}
    )
    
    add_dependencies(${TARGET_NAME} ${TARGET_NAME}_copy_resources)
endfunction()

copy_target_resources(${PROJECT_NAME} ${EDITOR_OUTPUT_DIR}
    SHADERS    ${SHADERS_DIR}
    FONTS      ${FONTS_DIR}
    MODELS     ${MODELS_DIR}
    TEXTURES   ${TEXTURES_DIR}
    ICONS      ${ICONS_DIR}
)

function(copy_target_dll target dll_path)
    get_target_property(TARGET_OUTPUT_DIR ${target} RUNTIME_OUTPUT_DIRECTORY)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${dll_path}"
            "${TARGET_OUTPUT_DIR}"
        COMMENT "复制依赖DLL到 ${target} 的输出目录"
    )
endfunction()

if(WIN32)
    if(MSVC)
        copy_target_dll(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/external/glfw/glfw3.dll")
        copy_target_dll(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/external/vulkan/lib/vulkan-1.dll")
        copy_target_dll(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/external/assimp/assimp/lib/assimp-vc143-mt.dll")
        copy_target_dll(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/external/vulkan/lib/shaderc/shaderc_sharedd.dll")
        #copy_target_dll(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/external/vulkan/lib/spirv_cross/spirv-cross-c-sharedd.dll")
    elseif(MINGW)
        copy_target_dll(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/external/glfw/libglfw3.a")
        copy_target_dll(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/external/vulkan/lib/vulkan-1.dll")
        copy_target_dll(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/external/assimp/assimp/lib/libassimp-6.dll")
        copy_target_dll(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/external/vulkan/lib/shaderc/shaderc_sharedd.dll")
        #copy_target_dll(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/external/vulkan/lib/spirv_cross/spirv-cross-c-sharedd.dll")
    endif()
elseif(UNIX)
        copy_target_dll(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/external/glfw/libglfw.so.3.3")
        copy_target_dll(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/external/vulkan/lib/libvulkan.so.1.4.304")
        copy_target_dll(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/external/assimp/assimp/lib/libassimp.so.5.3.0")
        copy_target_dll(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/external/vulkan/lib/shaderc/libshaderc_shared.so.1")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        ENABLE_VALIDATION_LAYERS
    )

    set(VK_LAYER_SRC "${CMAKE_SOURCE_DIR}/external/vulkan/lib/layers")
    
    if(CMAKE_RUNTIME_OUTPUT_DIRECTORY)
        set(VK_LAYER_DEST "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    else()
        set(VK_LAYER_DEST "${CMAKE_BINARY_DIR}/bin/${PROJECT_NAME}")
    endif()
    
    message(STATUS "VK_LAYER_SRC: ${VK_LAYER_SRC}")
    message(STATUS "VK_LAYER_DEST: ${VK_LAYER_DEST}")
    
    if(WIN32)
        # Windows: 复制验证层文件
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${VK_LAYER_DEST}/layers"        
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${VK_LAYER_SRC}/VkLayer_khronos_validation.dll"
                "${VK_LAYER_DEST}/"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${VK_LAYER_SRC}/VkLayer_khronos_validation.json"
                "${VK_LAYER_DEST}/layers"
            COMMENT "复制Vulkan验证层"
        )
        
    elseif(UNIX AND NOT APPLE)
        # Linux: 配置时复制
        file(MAKE_DIRECTORY "${VK_LAYER_DEST}/layers")
        file(COPY 
            "${VK_LAYER_SRC}/libVkLayer_khronos_validation.so"
            DESTINATION "${VK_LAYER_DEST}"
        )
        file(COPY 
            "${VK_LAYER_SRC}/libVkLayer_khronos_validation.json"
            DESTINATION "${VK_LAYER_DEST}/layers"
        )
        
        # 构建时也复制
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${VK_LAYER_DEST}/layers"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${VK_LAYER_SRC}/libVkLayer_khronos_validation.so"
                "${VK_LAYER_DEST}/"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${VK_LAYER_SRC}/libVkLayer_khronos_validation.json"
                "${VK_LAYER_DEST}/layers"
            COMMENT "复制Vulkan验证层"
        )
        
    endif()
    
    # 为所有平台创建简单的运行目标
    if(WIN32)
        add_custom_target(run
            COMMAND echo "运行 StarryEngine..."
            COMMAND "${VK_LAYER_DEST}/${PROJECT_NAME}.exe"
            DEPENDS ${PROJECT_NAME}
            COMMENT "运行程序"
        )
    else()
        add_custom_target(run
            COMMAND echo "运行 StarryEngine..."
            COMMAND "${VK_LAYER_DEST}/${PROJECT_NAME}"
            DEPENDS ${PROJECT_NAME}
            COMMENT "运行程序"
        )
    endif()
    
    message(STATUS "创建运行目标: make run")
endif()