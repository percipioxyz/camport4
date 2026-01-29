if(${CMAKE_VERSION} VERSION_LESS "3.5")
    cmake_minimum_required(VERSION 2.8)
else() # to avoid errors reported by CMake 4.0 and above versions
    cmake_minimum_required(VERSION 3.5)
endif()

option(BUILD_SAMPLE_V2_WITH_OPENCV "Enable opencv library dependencies " ON)

# ========================================
# === common, build static lib to speed up
# ========================================
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/hpp/)
include_directories(${COMMON_INC})

set(CPLUSPLUS_SAMPLE_API_SOURCE 
    cpp/Device.cpp
    cpp/Frame.cpp
    )

if (BUILD_SAMPLE_V2_WITH_OPENCV)
    find_package(OpenCV)
    if (NOT OpenCV_FOUND)
        message(WARNING "OpenCV library not found")
        set(BUILD_SAMPLE_V2_WITH_OPENCV OFF)
    else()
        include_directories(${OpenCV_INCLUDE_DIRS})
        include_directories(${OpenCV2_INCLUDE_DIRS})
        # if(CMAKE_BUILD_TYPE STREQUAL Debug)
        # link_directories(${OpenCV_LIB_DIR_DBG})
        # else()
        # link_directories(${OpenCV_LIB_DIR_OPT})
        # endif()
        link_directories(${OpenCV_LIB_DIR})
    endif()
endif()

add_library(cpp_api_lib STATIC ${COMMON_SOURCES} ${CPLUSPLUS_SAMPLE_API_SOURCE})
if (DEFINED TARGET_LIB_API)
  add_dependencies(cpp_api_lib ${TARGET_LIB_API})
endif()


if(UNIX)
    target_link_libraries(cpp_api_lib pthread)
endif()


add_subdirectory( sample )
