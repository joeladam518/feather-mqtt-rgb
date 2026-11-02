# The top level build file. These three lines are the standard start of an ESP-IDF project.
# The code itself is listed in src/CMakeLists.txt.
cmake_minimum_required(VERSION 3.22)

# ESP-IDF expects the app's code in a folder called main. Ours is called src, so the build
# has to be told where to find it. This has to come before the include below.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/src")

# Pulls in ESP-IDF's build system. IDF_PATH gets set by running export.sh.
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(feather-mqtt-rgb-tree)
