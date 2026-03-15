file(GLOB_RECURSE SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(scene STATIC ${SRC})

target_link_libraries(scene PRIVATE
	BaseInterface
	assets
)



