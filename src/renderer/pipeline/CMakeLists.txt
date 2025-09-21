file(GLOB_RECURSE SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(pipeline STATIC ${SRC})

target_link_libraries(pipeline PRIVATE
	BaseInterface
	core
	passes
	utils
	resources
)



