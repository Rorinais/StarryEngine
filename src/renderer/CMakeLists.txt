file(GLOB_RECURSE SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(renderer STATIC ${SRC})

target_link_libraries(renderer PRIVATE
	BaseInterface
)



