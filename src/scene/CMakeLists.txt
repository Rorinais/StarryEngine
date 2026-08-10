add_library(scene STATIC
    src/NonRenderObject.cpp
    src/Scene.cpp
    src/animation/Animator.cpp
    src/camera/CameraController.cpp
    src/camera/OrthographicCamera.cpp
    src/camera/PerspectiveCamera.cpp
)

target_include_directories(scene PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(scene
PUBLIC
    assets            # Scene 头引用 assets 类型
    core              # Scene 头引用 core 类型
    glm::glm
PRIVATE
    glfw              # CameraController.cpp 读 glfw 键位
    logging           # 仅 cpp 用
)
