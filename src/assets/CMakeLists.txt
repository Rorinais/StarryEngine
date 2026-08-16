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
    renderer          
    glm::glm
    assimp::assimp
    shaderc           
    spirv-cross-glsl  
    stb_image         
PRIVATE
    logging        
    utils             
)
