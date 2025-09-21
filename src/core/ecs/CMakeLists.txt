file(GLOB_RECURSE SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(ecs STATIC ${SRC})

target_link_libraries(ecs PRIVATE
	BaseInterface
	core
	passes
	utils
	resources
)