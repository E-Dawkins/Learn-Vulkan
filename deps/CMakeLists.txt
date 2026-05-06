# -------------------- GLFW --------------------
add_library(glfw STATIC IMPORTED GLOBAL)

set_target_properties(glfw PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/glfw/lib-vc2022/glfw3.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/glfw/include"
)

# -------------------- GLM --------------------
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}/glm
)

# -------------------- STB --------------------
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}/stb
)

# -------------------- TINYOBJ --------------------
add_library(tinyobj INTERFACE)
target_include_directories(tinyobj INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}/tinyobj
)

# -------------------- VULKAN_SDK --------------------
add_library(vulkan_sdk STATIC IMPORTED GLOBAL)

set_target_properties(vulkan_sdk PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/vulkan_sdk/Lib/vulkan-1.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/vulkan_sdk/Include"
)