cmake_minimum_required(VERSION 3.16.0)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Explicitly set main component
set(EXTRA_COMPONENT_DIRS "src")

project(cglp-m5stickc-plus-project)
