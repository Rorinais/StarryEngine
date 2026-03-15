
file(GLOB SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(utils STATIC ${SRC})

target_link_libraries(utils 
PRIVATE
    BaseInterface
)