file(GLOB_RECURSE SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(core STATIC ${SRC})

target_link_libraries(core PRIVATE
	BaseInterface
)



