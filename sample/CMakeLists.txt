if(${CMAKE_VERSION} VERSION_LESS "3.5")
    cmake_minimum_required(VERSION 2.8)
else() # to avoid errors reported by CMake 4.0 and above versions
    cmake_minimum_required(VERSION 3.5)
endif()
#used in common/imgdisplay
option(BUILD_SAMPLE_WITH_GUI "Build sample with image GUI library" ON)
if (NOT TARGET tycam)
    find_package(TYCam REQUIRED)
    include_directories(${TYCam_INCLUDE_DIRS})
    #check build Type
    if(CMAKE_BUILD_TYPE STREQUAL "")
        set(CMAKE_BUILD_TYPE Release CACHE STRING "Type of build, options are Debug Release" FORCE)
    endif()

    if(UNIX)
        #common flag, Will be override by Release/Debug if same flag setted
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -fvisibility-inlines-hidden -std=c++11")
        #Debug flag
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
        #Release flag
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -DNDEBUG -s")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG -s")
    endif()
    
    if(MSVC)
        add_definitions(-D_CRT_SECURE_NO_WARNINGS /wd"4819" /wd"4251" /wd"4275" /wd"4244" /wd"4267" /wd"4800" /wd"4305" /wd"4996")
        #freeglut static need define Linker flags
        set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /NODEFAULTLIB:libcmt.lib")
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /NODEFAULTLIB:libcmtd.lib")
    endif()
endif()

if (MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

option(BUILD_SAMPLE_V1 "Enable samle v1 build " ON)

option(BUILD_SAMPLE_V2 "Enable samle v2 build " ON)

option(BUILD_SAMPLE_GENICAM_SFNC "Enable samle genicam SFNC build " ON)
option (OpenCV_STATIC OFF)

#convert samples got from param to list
if (DEFINED SAMPLES)
  string(REPLACE " " ";" SAMPLES ${SAMPLES})
endif()

if (DEFINED BUILD_SAMPLES AND NOT BUILD_SAMPLES)
    set(BUILD_SAMPLE_V1 OFF)
    set(BUILD_SAMPLE_GENICAM_SFNC OFF)
endif()

if (DEFINED BUILD_CPLUSPLUS_SAMPLE AND NOT BUILD_CPLUSPLUS_SAMPLE)
    set(BUILD_SAMPLE_V2 OFF)
endif()

if (DEFINED BUILD_GENICAM_SFNC_SAMPLE AND NOT BUILD_GENICAM_SFNC_SAMPLE)
    set(BUILD_SAMPLE_GENICAM_SFNC OFF)
endif()

if(BUILD_SAMPLE_WITH_GUI)
    # ========================================
    # === OpenCV First
    # ========================================
    find_package(OpenCV)
    if (OpenCV_FOUND)
        include_directories(${OpenCV_INCLUDE_DIRS})
        include_directories(${OpenCV2_INCLUDE_DIRS})
        link_directories(${OpenCV_LIB_DIR})
        set(COMMON_GUI_LIBS ${OpenCV_LIBS})
        if (MSVC)
            if (OpenCV_STATIC)
                message("using static opencv")
                set(CompilerFlags
                    CMAKE_CXX_FLAGS
                    CMAKE_CXX_FLAGS_DEBUG
                    CMAKE_CXX_FLAGS_RELEASE
                    CMAKE_C_FLAGS
                    CMAKE_C_FLAGS_DEBUG
                    CMAKE_C_FLAGS_RELEASE
                    )
                foreach(CompilerFlag ${CompilerFlags})
                    string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
                endforeach()
            endif()
        endif()
     endif()
endif()

set (COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/common)
add_subdirectory(common/imgdisplay)
message(STATUS  "COMMON_GUI_LIBS ${COMMON_GUI_LIBS}")
message(STATUS "COMMON_GUI_SRC ${COMMON_GUI_SRC}")
set (COMMON_INC ${COMMON_DIR} ${COMMON_GUI_INC})
set (COMMON_SOURCES
    ${COMMON_DIR}/TYThread.cpp
    ${COMMON_DIR}/crc32.cpp
    ${COMMON_DIR}/json11.cpp
    ${COMMON_DIR}/ParametersParse.cpp
    ${COMMON_DIR}/huffman.cpp
    ${COMMON_DIR}/FileAccessControl.cpp)

list(APPEND COMMON_SOURCES ${COMMON_GUI_SRC})
message(STATUS "COMMON_SOURCES ${COMMON_SOURCES}")

if (MSVC)#for windows
    set (LIB_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../lib/win/hostapp/)
endif()
set (CLOUD_VIEW_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/cloud_viewer/")
file (GLOB CLOUD_VIEW_SRC ${CLOUD_VIEW_DIR}/*.c ${CLOUD_VIEW_DIR}/*.cpp )
set (CLOUD_VIEW_INC ${CLOUD_VIEW_DIR}/freeglut/include)

if (BUILD_SAMPLE_V1)
    message(STATUS "sample v1 ON ")
    add_subdirectory(sample_v1)
endif()

if (BUILD_SAMPLE_V2)
    message(STATUS "sample v2 ON ")
    add_subdirectory(sample_v2)
endif()

if (BUILD_SAMPLE_GENICAM_SFNC)
    message(STATUS "sample Gen<I>Cam SFNC ON ")
    add_subdirectory(sample_genicam_sfnc)
endif()
