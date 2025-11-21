if(${CMAKE_VERSION} VERSION_LESS "3.5")
    cmake_minimum_required(VERSION 2.8)
else() # to avoid errors reported by CMake 4.0 and above versions
    cmake_minimum_required(VERSION 3.5)
endif()

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
  sample_get_rw_api
  sample_speckleFilter
  sample_file_iostream
  sample_reconnect
  )

set(UserSetCtrlSample
    UserSetControl/sample_UserSetCurrent
    UserSetControl/sample_UserSetDefault
    UserSetControl/sample_UserSetDescription
    UserSetControl/sample_UserSetLoad
    UserSetControl/sample_UserSetSave
    UserSetControl/sample_UserSetSelector
)

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
    if (DEFINED SAMPLES)
      list(FIND SAMPLES ${sample_path} index)
      if(index EQUAL -1)
        continue()
      endif()
    endif()
    get_filename_component(spath "${sample_path}" ABSOLUTE )
    if (EXISTS "${spath}/")
        file(GLOB sources ${sample_path}/*.cpp)
        get_filename_component(sample ${sample_path} NAME)
        add_executable(${sample} ${sources})
        add_dependencies(${sample} sample_genicam_sfnc_common ${TARGET_LIB})
        target_link_libraries(${sample} sample_genicam_sfnc_common ${TYCam_LIBS}  ${OpenCV_LIBS} ${CLOUD_VIEWER})
        set_target_properties(${sample} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/)
        set_target_properties(${sample} PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON )
        install(TARGETS ${sample} RUNTIME DESTINATION samples/)
    endif()
endforeach()

