add_library(assets STATIC
    src/animation/AnimationClip.cpp
    src/animation/Skeleton.cpp
    src/geometry/Geometry.cpp
    src/geometry/GeometryGenerator.cpp
    src/geometry/VertexLayout.cpp
    src/loader/IBLBuilder.cpp
    src/loader/ModelLoader.cpp
    src/loader/ShaderLoader.cpp
    src/loader/TextureLoader.cpp
    src/material/DefaultMaterialTemplate.cpp
    src/material/MaterialInstance.cpp
    src/material/MaterialParameterBlock.cpp
    src/material/MaterialTemplate.cpp
)

target_include_directories(assets PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(assets
PUBLIC
    renderer          # 各 Asset 头引用 renderer 类型
    glm::glm
    assimp::assimp
    shaderc           # ShaderLoader.hpp 引用 shaderc 头
    spirv-cross-glsl  # ShaderLoader.hpp 引用 spirv_cross 头（传递 spirv-cross-core）
    stb_image         # TextureLoader 单头
PRIVATE
    logging           # 仅 cpp 用
    utils             # 仅 cpp 用
)
