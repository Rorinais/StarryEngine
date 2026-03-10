
file(GLOB SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(logging STATIC ${SRC})

target_link_libraries(logging 
PRIVATE
    BaseInterface
    core
)