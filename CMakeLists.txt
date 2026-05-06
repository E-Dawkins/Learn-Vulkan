cmake_minimum_required(VERSION 3.25)

project(LearnVulkan LANGUAGES CXX)

# add directory for third-party libraries / dependencies
add_subdirectory(deps)

# add directories for solution projects
add_subdirectory(LearnVulkan_Project)