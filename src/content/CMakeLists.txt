# L4 内容层：场景装配（PBR 场景、光追场景）。只依赖引擎 + app 的配置接口。
add_library(content STATIC
    src/render/RendererSwitcher.cpp
    src/render/materials/BuiltinMaterials.cpp
    src/render/materials/CharacterMaterials.cpp
    src/renderpath/RenderPathRegistry.cpp
    src/renderpath/StandardPipeline.cpp
    src/scene/SceneBuilder.cpp
    src/scenes/pbr/PbrScene.cpp
    src/scenes/pbr/PbrEnvironment.cpp
    src/scenes/pbr/PbrWorld.cpp
    src/scenes/pbr/PbrMaterials.cpp
)
target_include_directories(content PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(content
PUBLIC
    renderer assets scene app application
PRIVATE
    logging core
)
