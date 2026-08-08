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
  sample_file_iostream
  sample_reconnect
  sample_view_registration
  sample_region_ctrl
  sample_event
  sample_watermark
  sample_seqctrl
  sample_split_ctrl
  sample_split_stream
  )

# Samples that require OpenGL/GLUT (cloud point viewer)
set(SAMPLES_USED_GLUT
  sample_view_point3d
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
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${COMMON_INC}/)
add_library(sample_genicam_sfnc_common STATIC ${COMMON_SOURCES})
if(UNIX)
  target_link_libraries(sample_genicam_sfnc_common pthread)
endif()

# ========================================
# === OpenGL (cloud viewer) support
# ========================================
if (OPENGL_VIEW)
    if (NOT TARGET cloud_viewer)
        include_directories(${CLOUD_VIEW_INC})
        add_library(cloud_viewer STATIC ${CLOUD_VIEW_SRC})
        target_compile_definitions(cloud_viewer PUBLIC FREEGLUT_STATIC)
        if (WIN32)
            include(ExternalProject)
            ExternalProject_Add( glut_proj_sfnc
                PREFIX "${CMAKE_CURRENT_SOURCE_DIR}"
                SOURCE_DIR "${CLOUD_VIEW_DIR}/freeglut"
                BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/freeglut_build"
                CMAKE_ARGS
                -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/freeglut_install
                -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                )
            add_library(myglut_sfnc STATIC IMPORTED)
            add_dependencies(myglut_sfnc glut_proj_sfnc)
            file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/freeglut_install/include)
            set_target_properties(myglut_sfnc PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_BINARY_DIR}/freeglut_install/include)
            set_property(TARGET myglut_sfnc PROPERTY IMPORTED_LOCATION
                ${CMAKE_CURRENT_BINARY_DIR}/freeglut_install/lib/freeglut_static.lib)
            add_dependencies(cloud_viewer glut_proj_sfnc)
            target_link_libraries(cloud_viewer PUBLIC myglut_sfnc)
        else()
            find_package(GLUT REQUIRED)
            find_package(OpenGL REQUIRED)
            include_directories(${GLUT_INCLUDE_DIR} ${OPENGL_INCLUDE_DIRS})
            target_link_libraries(cloud_viewer PUBLIC ${GLUT_LIBRARIES} ${OPENGL_LIBRARIES})
        endif()
    else()
        find_package(GLUT QUIET)
        find_package(OpenGL QUIET)
    endif()
    set(CLOUD_VIEWER ${GLUT_LIBRARIES} ${OPENGL_LIBRARIES} cloud_viewer)
    include_directories(${CLOUD_VIEW_DIR})
    set(ALL_SAMPLES ${ALL_SAMPLES} ${SAMPLES_USED_GLUT})
endif()

set(ALL_SAMPLES ${ALL_SAMPLES} ${UserSetCtrlSample})

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
        target_link_libraries(${sample} sample_genicam_sfnc_common ${TYCam_LIBS} ${COMMON_GUI_LIBS} ${CLOUD_VIEWER})
        set_target_properties(${sample} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/)
        set_target_properties(${sample} PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON )
        install(TARGETS ${sample} RUNTIME DESTINATION samples/)
    endif()
endforeach()


