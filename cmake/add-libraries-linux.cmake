include_directories("/usr/local/include")
include_directories("${CMAKE_SOURCE_DIR}/3rd_party/glfw3/include")
include_directories("${CMAKE_SOURCE_DIR}/3rd_party/glm")
include_directories("${CMAKE_SOURCE_DIR}/3rd_party/freeimage/include")

link_directories("/usr/local/lib")
link_directories("${CMAKE_SOURCE_DIR}/3rd_party/glad/lib/linux")
link_directories("${CMAKE_SOURCE_DIR}/3rd_party/glfw3/lib/linux")
link_directories("${CMAKE_SOURCE_DIR}/3rd_party/freeimage/lib/linux")