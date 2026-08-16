add_library(scene STATIC
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
    assets            
    core              
    glm::glm
PRIVATE
    glfw              
    logging          
)
