
file(GLOB SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(event STATIC ${SRC})

target_link_libraries(event 
PRIVATE
    BaseInterface
    core
    logging
)