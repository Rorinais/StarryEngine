file(GLOB_RECURSE SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(resources STATIC ${SRC})

target_link_libraries(resources PRIVATE
	BaseInterface
	core
)



