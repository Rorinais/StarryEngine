
file(GLOB SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(application STATIC ${SRC})

target_link_libraries(application 
PRIVATE
    BaseInterface
    window
    renderer
)