
file(GLOB SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(window STATIC ${SRC})

target_link_libraries(window 
PRIVATE
    BaseInterface
)