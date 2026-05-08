file(GLOB_RECURSE SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(ui STATIC ${SRC})

target_link_libraries(ui PRIVATE
	BaseInterface
	core
	renderer
)



