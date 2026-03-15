
file(GLOB_RECURSE SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(assets STATIC ${SRC})

target_link_libraries(assets 
PRIVATE
    BaseInterface
)