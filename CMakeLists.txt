cmake_minimum_required(VERSION 3.16)

# Make the repository root directly build the end-device firmware while keeping
# the existing modular source tree in node-enddevice-navigation/main.
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/node-enddevice-navigation/main"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(node-enddevice-navigation)
