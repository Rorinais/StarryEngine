# L4 内容层：场景装配（PBR 场景、光追场景）。只依赖引擎 + app 的配置接口。
add_library(content STATIC
    src/creation/SceneSwitcher.cpp
    src/pbr/PbrScene.cpp
    src/pbr/PbrEnvironment.cpp
    src/pbr/PbrWorld.cpp
    src/pbr/PbrMaterials.cpp
    src/materials/BuiltinMaterials.cpp
    src/creation/SceneBuilder.cpp
    src/creation/RenderPathRegistry.cpp
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
