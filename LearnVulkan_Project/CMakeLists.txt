# change solution project name here
set(CURRENT_PROJECT "LearnVulkan_Project")

# find and store all header (*.h) and source (*.cpp) files
file(GLOB_RECURSE PROJECT_HEADERS CONFIGURE_DEPENDS inc/*.h)
file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS src/*.cpp)

# create project
add_executable(${CURRENT_PROJECT} ${PROJECT_HEADERS} ${PROJECT_SOURCES})

# add root include directory
target_include_directories(${CURRENT_PROJECT} PRIVATE inc)

# find + link Vulkan from system, this is so we do not have to source control a massive SDK
find_package(Vulkan REQUIRED)
target_link_libraries(${CURRENT_PROJECT} PRIVATE Vulkan::Vulkan)

# link any dependencies to project
target_link_libraries(${CURRENT_PROJECT} PRIVATE 
    glfw
    glm
    stb
    tinyobj
)

# Create filters based on file paths (*.h *.cpp)
foreach(_file IN LISTS PROJECT_HEADERS PROJECT_SOURCES)
    # Get relative path from project root
    file(RELATIVE_PATH _relPath "${CMAKE_SOURCE_DIR}" "${_file}")

    # Extract directory and convert to filter format
    get_filename_component(_dir "${_relPath}" DIRECTORY)
    string(REPLACE "/" "\\" _filter "${_dir}")

    # Assign to filter
    source_group("${_filter}" FILES "${_file}")
endforeach()

# set project to use c++ 20
set_target_properties(${CURRENT_PROJECT} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

# set warning level 4 and treat warnings as errors
target_compile_options(${CURRENT_PROJECT} PRIVATE /W4 /WX)

if (MSVC)
    # set external includes on and to have warnings disabled
    target_compile_options(${CURRENT_PROJECT} PRIVATE /external:anglebrackets /external:W0)

    # set project as vs startup project
    set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${CURRENT_PROJECT})

    # set working directory to cmake source directory
    set_property(TARGET ${CURRENT_PROJECT} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()