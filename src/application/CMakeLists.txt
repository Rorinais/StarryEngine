add_library(application STATIC
    src/Application.cpp
)

target_include_directories(application PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(application
PUBLIC
    assets
    core
    event
    logging
    renderer
    scene
    ui
    glfw
    imgui
)
