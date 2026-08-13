
function(copy_target_resources TARGET_NAME RESOURCES_OUTPUT_DIR)
    cmake_parse_arguments(ARG "" "SHADERS;FONTS;MODELS;TEXTURES;ICONS;CONFIG;MATERIAL" "" ${ARGN})

    get_target_property(TARGET_OUTPUT_DIR ${TARGET_NAME} RUNTIME_OUTPUT_DIRECTORY)
    set(ASSETS_DEST "${TARGET_OUTPUT_DIR}/assets")

    file(MAKE_DIRECTORY "${ASSETS_DEST}")

    set(ALL_RESOURCE_FILES "")
    set(ALL_COPY_COMMANDS "")

    if(ARG_CONFIG)
        file(GLOB_RECURSE CONFIG_FILES CONFIGURE_DEPENDS
            "${ARG_CONFIG}/*.json"
            "${ARG_CONFIG}/*.yaml"
            "${ARG_CONFIG}/*.ini"
            "${ARG_CONFIG}/*.cfg"
        )
        foreach(cfg_file IN LISTS CONFIG_FILES)
            file(RELATIVE_PATH relative_path "${ARG_CONFIG}" "${cfg_file}")
            set(final_dest "${ASSETS_DEST}/configs/${relative_path}")
            get_filename_component(final_dir "${final_dest}" DIRECTORY)
            list(APPEND ALL_COPY_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E make_directory "${final_dir}"
            )
            list(APPEND ALL_COPY_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${cfg_file}"
                    "${final_dest}"
            )
            list(APPEND ALL_RESOURCE_FILES "${final_dest}")
        endforeach()
    endif()

    if(ARG_MATERIAL)
        file(GLOB_RECURSE MATERIAL_FILES CONFIGURE_DEPENDS
            "${ARG_MATERIAL}/*.json"
            "${ARG_MATERIAL}/*.mat"
            "${ARG_MATERIAL}/*.cfg"
        )
        foreach(mat_file IN LISTS MATERIAL_FILES)             
            file(RELATIVE_PATH relative_path "${ARG_MATERIAL}" "${mat_file}")
            set(final_dest "${ASSETS_DEST}/materials/${relative_path}")
            get_filename_component(final_dir "${final_dest}" DIRECTORY)
            list(APPEND ALL_COPY_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E make_directory "${final_dir}"
            )
            list(APPEND ALL_COPY_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${mat_file}" 
                    "${final_dest}"
            )
            list(APPEND ALL_RESOURCE_FILES "${final_dest}")
        endforeach()
    endif()

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
            "${ARG_MODELS}/*.png"
            "${ARG_MODELS}/*.jpg"
            "${ARG_MODELS}/*.jpeg"
            "${ARG_MODELS}/*.hdr"
            "${ARG_MODELS}/*.tga"
            "${ARG_MODELS}/*.bmp"
            "${ARG_MODELS}/*.ktx"
            "${ARG_MODELS}/*.dds"
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
        DEPENDS ${SHADER_FILES} ${ICON_FILES} ${FONT_FILES} ${MODEL_FILES} ${TEXTURE_FILES} ${CONFIG_FILES} ${MATERIAL_FILES}
        VERBATIM
    )
    add_custom_target(${TARGET_NAME}_copy_resources ALL
        DEPENDS ${ALL_RESOURCE_FILES}
    )
    add_dependencies(${TARGET_NAME} ${TARGET_NAME}_copy_resources)
endfunction()

function(copy_target_dll target dll_path)
    get_target_property(TARGET_OUTPUT_DIR ${target} RUNTIME_OUTPUT_DIRECTORY)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${dll_path}"
            "${TARGET_OUTPUT_DIR}"
        COMMENT "复制依赖DLL到 ${target} 的输出目录"
    )
endfunction()

function(add_engine_executable)
    set(options)
    set(oneValueArgs TARGET_NAME OUTPUT_DIR SHADERS_DIR FONTS_DIR MODELS_DIR TEXTURES_DIR ICONS_DIR ICON_FILE )
    set(multiValueArgs SOURCES)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT ARG_TARGET_NAME)
        message(FATAL_ERROR "TARGET_NAME is required")
    endif()
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "SOURCES is required")
    endif()

    if(NOT ARG_OUTPUT_DIR)
        set(ARG_OUTPUT_DIR "${CMAKE_BINARY_DIR}/${ARG_TARGET_NAME}")
    endif()

    if(NOT ARG_SHADERS_DIR)
        set(ARG_SHADERS_DIR "${CMAKE_SOURCE_DIR}/../StarryEngine/assets/shaders")
    endif()
    if(NOT ARG_MODELS_DIR)
        set(ARG_MODELS_DIR "${CMAKE_SOURCE_DIR}/../StarryEngine/assets/models")
    endif()
    if(NOT ARG_TEXTURES_DIR)
        set(ARG_TEXTURES_DIR "${CMAKE_SOURCE_DIR}/../StarryEngine/assets/textures")
    endif()
    if(NOT ARG_FONTS_DIR)
        set(ARG_FONTS_DIR "${CMAKE_SOURCE_DIR}/../StarryEngine/assets/fonts")
    endif()
    if(NOT ARG_ICONS_DIR)
        set(ARG_ICONS_DIR "${CMAKE_SOURCE_DIR}/../StarryEngine/assets/icons")
    endif()
    if(NOT ARG_CONFIG_DIR)
        set(ARG_CONFIG_DIR "${CMAKE_SOURCE_DIR}/../StarryEngine/assets/configs")
    endif()
    if(NOT ARG_MATERIAL_DIR)
        set(ARG_MATERIAL_DIR "${CMAKE_SOURCE_DIR}/../StarryEngine/assets/materials")
    endif()
    if(NOT ARG_ICON_FILE)
        set(ARG_ICON_FILE "${ARG_ICONS_DIR}/app_icon.ico")
    endif()

    # 生成图标资源文件
    if(NOT EXISTS "${ARG_ICON_FILE}")
        message(WARNING "图标文件不存在: ${ARG_ICON_FILE}")
        file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/app_icon.ico "")
    else()
        configure_file(${ARG_ICON_FILE} ${CMAKE_CURRENT_BINARY_DIR}/app_icon.ico COPYONLY)
    endif()
    set(RC_CONTENT "IDI_ICON1 ICON \"app_icon.ico\"\n")
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/generated.rc "${RC_CONTENT}")

    add_executable(${ARG_TARGET_NAME} ${ARG_SOURCES} ${CMAKE_CURRENT_BINARY_DIR}/generated.rc)

    set_target_properties(${ARG_TARGET_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${ARG_OUTPUT_DIR}
    )

    if(UNIX AND NOT APPLE)
        target_link_options(${ARG_TARGET_NAME} PRIVATE "-Wl,--disable-new-dtags")
    endif()

    # 引擎模块库：重构按里程碑逐个 add_subdirectory 加入。
    # 这里只链接"已存在"的模块 target，避免骨架阶段链接到尚不存在的库
    # （否则 ld: cannot find -lcore / -lrenderer ...）。模块加入后自动进入链接组。
    set(_se_modules core assets scene event utils logging renderer ui)
    set(_se_modules_link "")
    foreach(_se_mod IN LISTS _se_modules)
        if(TARGET ${_se_mod})
            list(APPEND _se_modules_link ${_se_mod})
        endif()
    endforeach()

    if(_se_modules_link)
        target_link_libraries(${ARG_TARGET_NAME} PRIVATE
            application
            "$<LINK_GROUP:RESCAN,${_se_modules_link}>"
        )
    else()
        target_link_libraries(${ARG_TARGET_NAME} PRIVATE
            application
        )
    endif()

    copy_target_resources(${ARG_TARGET_NAME} ${ARG_OUTPUT_DIR}
        SHADERS    ${ARG_SHADERS_DIR}
        FONTS      ${ARG_FONTS_DIR}
        MODELS     ${ARG_MODELS_DIR}
        TEXTURES   ${ARG_TEXTURES_DIR}
        ICONS      ${ARG_ICONS_DIR}
        CONFIG     ${ARG_CONFIG_DIR}
        MATERIAL   ${ARG_MATERIAL_DIR}
    )

    if(WIN32)
        if(MSVC)
            copy_target_dll(${ARG_TARGET_NAME} "${CMAKE_SOURCE_DIR}/external/glfw/lib/glfw3.dll")
            copy_target_dll(${ARG_TARGET_NAME} "${CMAKE_SOURCE_DIR}/external/vulkan/lib/vulkan-1.dll")
            copy_target_dll(${ARG_TARGET_NAME} "${CMAKE_SOURCE_DIR}/external/assimp/lib/assimp-vc143-mt.dll")
            copy_target_dll(${ARG_TARGET_NAME} "${CMAKE_SOURCE_DIR}/external/vulkan/lib/shaderc/shaderc_shared.dll")
            copy_target_dll(${ARG_TARGET_NAME} "${CMAKE_SOURCE_DIR}/external/vulkan/lib/shaderc/shaderc_sharedd.dll")
        elseif(MINGW)
            copy_target_dll(${ARG_TARGET_NAME} "${CMAKE_SOURCE_DIR}/external/glfw/lib/libglfw3.a")
            copy_target_dll(${ARG_TARGET_NAME} "${CMAKE_SOURCE_DIR}/external/vulkan/lib/vulkan-1.dll")
            copy_target_dll(${ARG_TARGET_NAME} "${CMAKE_SOURCE_DIR}/external/assimp/lib/libassimp-6.dll")
            copy_target_dll(${ARG_TARGET_NAME} "${CMAKE_SOURCE_DIR}/external/vulkan/lib/shaderc/shaderc_shared.dll")
            copy_target_dll(${ARG_TARGET_NAME} "${CMAKE_SOURCE_DIR}/external/vulkan/lib/shaderc/shaderc_sharedd.dll")
        endif()
    elseif(UNIX)
        copy_target_dll(${ARG_TARGET_NAME} "${CMAKE_SOURCE_DIR}/external/glfw/lib/libglfw.so.3.4")
        copy_target_dll(${ARG_TARGET_NAME} "${CMAKE_SOURCE_DIR}/external/vulkan/lib/libvulkan.so.1.4.304")
        copy_target_dll(${ARG_TARGET_NAME} "${CMAKE_SOURCE_DIR}/external/vulkan/lib/shaderc/libshaderc_shared.so.1")
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(${ARG_TARGET_NAME} PRIVATE ENABLE_VALIDATION_LAYERS)

        set(VK_LAYER_SRC "${CMAKE_SOURCE_DIR}/external/vulkan/lib/layers")
        set(VK_LAYER_DEST "${ARG_OUTPUT_DIR}")

        if(WIN32)
            add_custom_command(TARGET ${ARG_TARGET_NAME} POST_BUILD
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
            add_custom_command(TARGET ${ARG_TARGET_NAME} POST_BUILD
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

        add_custom_target(run_${ARG_TARGET_NAME}
            COMMAND echo "Running ${ARG_TARGET_NAME}..."
            COMMAND "${VK_LAYER_DEST}/${ARG_TARGET_NAME}${CMAKE_EXECUTABLE_SUFFIX}"
            DEPENDS ${ARG_TARGET_NAME}
            COMMENT "Run ${ARG_TARGET_NAME}"
        )
    endif()
endfunction()