cmake_minimum_required(VERSION 2.8)

set(ALL_SAMPLES
  sample_stream
  sample_rw_feature
  sample_binocular_stream
  sample_parameters_laser
  sample_parameters_depthrange
  sample_parameters_exp_time
  sample_read_calibration_data
  sample_userset_ctl
  sample_trigger
  sample_ForceDeviceIP
  )

set(UserSetCtrlSample
    UserSetControl/sample_UserSetCurrent
    UserSetControl/sample_UserSetDefault
    UserSetControl/sample_UserSetDescription
    UserSetControl/sample_UserSetLoad
    UserSetControl/sample_UserSetSave
    UserSetControl/sample_UserSetSelector
)

if (MSVC)
    option(OpenCV_STATIC "Using static opencv" OFF)
endif()

if (NOT TARGET tycam) 
    #only build samples 
    option (OpenCV_STATIC OFF)
    set(INCLUDE_PATH ../../include)
    include_directories(${INCLUDE_PATH})
    set(ABSOLUTE_TYCAM_LIB tycam)
    add_library(${ABSOLUTE_TYCAM_LIB} SHARED IMPORTED)
    if (MSVC)#for windows
        if(CMAKE_CL_64) #x64
            set_property(TARGET ${ABSOLUTE_TYCAM_LIB} PROPERTY IMPORTED_LOCATION ${LIB_ROOT_PATH}/x64/tycam.dll)
            set_property(TARGET ${ABSOLUTE_TYCAM_LIB} PROPERTY IMPORTED_IMPLIB  ${LIB_ROOT_PATH}/x64/tycam.lib)
        else()
            set_property(TARGET ${ABSOLUTE_TYCAM_LIB} PROPERTY IMPORTED_LOCATION ${LIB_ROOT_PATH}/x86/tycam.dll)
            set_property(TARGET ${ABSOLUTE_TYCAM_LIB} PROPERTY IMPORTED_IMPLIB ${LIB_ROOT_PATH}/x86/tycam.lib)
        endif()
    else()
      if(ARCH)
          set_property(TARGET ${ABSOLUTE_TYCAM_LIB} PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/../lib/linux/lib_${ARCH}/libtycam.so)
      else()
          set(ABSOLUTE_TYCAM_LIB -ltycam)
      endif()
    endif()
endif()

if (NOT TARGET tyimgproc)
    option (OpenCV_STATIC OFF)
    set(ABSOLUTE_TYIMGPROC_LIB tyimgproc)
    add_library(${ABSOLUTE_TYIMGPROC_LIB} SHARED IMPORTED)
    if (MSVC)#for windows
        if(CMAKE_CL_64) #x64
            set_property(TARGET ${ABSOLUTE_TYIMGPROC_LIB} PROPERTY IMPORTED_LOCATION ${LIB_ROOT_PATH}/x64/tyimgproc.dll)
            set_property(TARGET ${ABSOLUTE_TYIMGPROC_LIB} PROPERTY IMPORTED_IMPLIB  ${LIB_ROOT_PATH}/x64/tyimgproc.lib)
        else()
            set_property(TARGET ${ABSOLUTE_TYIMGPROC_LIB} PROPERTY IMPORTED_LOCATION ${LIB_ROOT_PATH}/x86/tyimgproc.dll)
            set_property(TARGET ${ABSOLUTE_TYIMGPROC_LIB} PROPERTY IMPORTED_IMPLIB ${LIB_ROOT_PATH}/x86/tyimgproc.lib)
        endif()
    else()
        if(ARCH)
            set_property(TARGET ${ABSOLUTE_TYIMGPROC_LIB} PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/../lib/linux/lib_${ARCH}/tyimgproc.so)
        else()
            set(ABSOLUTE_TYIMGPROC_LIB -ltyimgproc)
        endif()
    endif()
endif()

if (MSVC)
    if (OpenCV_STATIC)
        message("using static opencv")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
    endif()
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

# ========================================
# === common, build static lib to speed up
# ========================================
include_directories(${COMMON_INC}/)
add_library(sample_genicam_sfnc_common STATIC ${COMMON_SOURCES})
if(UNIX)
  target_link_libraries(sample_genicam_sfnc_common pthread)
endif()

add_definitions(-DOPENCV_DEPENDENCIES)

set(ALL_SAMPLES ${ALL_SAMPLES} ${UserSetCtrlSample})

# ========================================
# === OpenCV
# ========================================
find_package(OpenCV REQUIRED)
if (NOT OpenCV_FOUND)
    message(FATAL_ERROR "OpenCV library not found")
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

add_definitions(-DOPENCV_DEPENDENCIES)
foreach(sample_path ${ALL_SAMPLES})
    get_filename_component(spath "${sample_path}" ABSOLUTE )
    if (EXISTS "${spath}/")
        file(GLOB sources ${sample_path}/*.cpp)
        get_filename_component(sample ${sample_path} NAME)
        add_executable(${sample} ${sources})
        add_dependencies(${sample} sample_genicam_sfnc_common ${TARGET_LIB})
        target_link_libraries(${sample} sample_genicam_sfnc_common ${ABSOLUTE_TYCAM_LIB}  ${OpenCV_LIBS} ${CLOUD_VIEWER})
        if(BUILD_IMGPROC)
            target_link_libraries(${sample} ${ABSOLUTE_TYIMGPROC_LIB})
        endif()
        set_target_properties(${sample} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/)
        set_target_properties(${sample} PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON )
        install(TARGETS ${sample} RUNTIME DESTINATION samples/)
    endif()
endforeach()

