if(${CMAKE_VERSION} VERSION_LESS "3.5")
    cmake_minimum_required(VERSION 2.8)
else() # to avoid errors reported by CMake 4.0 and above versions
    cmake_minimum_required(VERSION 3.5)
endif()

set(ALL_SAMPLES
    DumpAllFeatures
    DumpCalibInfo
    ListDevices
    LoopDetect
    ForceDeviceIP
    NetStatistic
    DeviceStorage
    SimpleView_FetchFrame
    SimpleView_Callback
    SimpleView_Registration
    SimpleView_PixelsRegistration
    SimpleView_FetchHisto
    SimpleView_MultiDevice
    SimpleView_TriggerMode0
    SimpleView_TriggerMode1
    SimpleView_TriggerMode20
    SimpleView_TriggerMode21
    SimpleView_TriggerMode22
    SimpleView_TriggerMode28
    SimpleView_TriggerMode29
    SimpleView_TriggerMode30
    SimpleView_TriggerMode31
    SimpleView_TriggerMode_M2S1
    SimpleView_TriggerMode_M3S1
    SimpleView_KeepAlive
    SimpleView_TriggerDelay
    SimpleView_StreamAsync
    SimpleView_StreamAsync1
    SimpleView_ImuData
    SimpleView_DI_DO
    SimpleView_OpenWithIP
    SimpleView_HDR
    SimpleView_SGBM
    SimpleView_XYZ48
    SimpleView_SaveLoadConfig
    )

set(SAMPLES_USED_GLUT
    SimpleView_Point3D
    SimpleView_Point3D_XYZ48
    )

option(OPENGL_VIEW "Enable GLUT OpenGL point cloud view " OFF)

# ========================================
# === common, build static lib to speed up
# ========================================
include_directories(${COMMON_INC}/)
add_library(sample_common STATIC ${COMMON_SOURCES})
if (DEFINED TARGET_LIB_API)
  add_dependencies(sample_common ${TARGET_LIB_API})
endif()
if(UNIX)
  target_link_libraries(sample_common pthread)
endif()

# ========================================
# === OpenGL
# ========================================
if (OPENGL_VIEW)
    include_directories(${CLOUD_VIEW_INC})
    add_library(cloud_viewer STATIC ${CLOUD_VIEW_SRC})
    target_compile_definitions(cloud_viewer PUBLIC FREEGLUT_STATIC)

    if (WIN32)
        #build freeglut by ourselves
        include(ExternalProject)
        ExternalProject_Add( glut_proj
            PREFIX "${CMAKE_CURRENT_SOURCE_DIR}"
            SOURCE_DIR "${CLOUD_VIEW_DIR}/freeglut"
            BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/freeglut_build"
            CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/freeglut_install
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
            -DCMAKE_STATIC_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}
            -DCMAKE_STATIC_LINKER_FLAGS_DEBUG=${CMAKE_EXE_LINKER_FLAGS_DEBUG}
            -DCMAKE_STATIC_LINKER_FLAGS_RELEASE=${CMAKE_EXE_LINKER_FLAGS_RELEASE}
            -DCMAKE_CXX_FLAGS_RELEASE=${CMAKE_CXX_FLAGS_RELEASE}
            -DCMAKE_CXX_FLAGS_DEBUG=${CMAKE_CXX_FLAGS_DEBUG}
            -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
            -DCMAKE_C_FLAGS_DEBUG=${CMAKE_C_FLAGS_DEBUG}
            -DCMAKE_C_FLAGS_RELEASE=${CMAKE_C_FLAGS_RELEASE}
            )

        add_library(myglut STATIC IMPORTED)
        add_dependencies(myglut glut_proj)
        file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/freeglut_install/include)
        set_target_properties(myglut PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_BINARY_DIR}/freeglut_install/include)
        set( glut_lib_file ${CMAKE_CURRENT_BINARY_DIR}/freeglut_install/lib/freeglut_staticd.lib)
        set_property(TARGET myglut PROPERTY IMPORTED_LOCATION_DEBUG ${glut_lib_file} )
        set( glut_lib_file ${CMAKE_CURRENT_BINARY_DIR}/freeglut_install/lib/freeglut_static.lib)
        set_property(TARGET myglut PROPERTY IMPORTED_LOCATION ${glut_lib_file} )

        add_dependencies(cloud_viewer glut_proj)
        target_link_libraries(cloud_viewer PUBLIC myglut)
    else()
        find_package(GLUT REQUIRED)
        find_package(OpenGL REQUIRED)
        include_directories(${GLUT_INCLUDE_DIR} ${OPENGL_INCLUDE_DIRS})
        target_link_libraries(cloud_viewer PUBLIC ${GLUT_LIBRARIES} ${OPENGL_LIBRARIES})
    endif()
    include_directories(cloud_viewer)
    set(ALL_SAMPLES ${ALL_SAMPLES} ${SAMPLES_USED_GLUT})
    set(CLOUD_VIEWER  ${GLUT_LIBRARIES} cloud_viewer)

endif()

#install for sample header file

foreach(sample ${ALL_SAMPLES})
    if (DEFINED SAMPLES)
      list(FIND SAMPLES ${sample} index)
      if(index EQUAL -1)
        continue()
      endif()
    endif()
    get_filename_component(spath "${sample}" ABSOLUTE )
    if (EXISTS "${spath}/")
        file(GLOB sources ${sample}/*.cpp)
        add_executable(${sample} ${sources})
        add_dependencies(${sample} sample_common ${TARGET_LIB})
        target_link_libraries(${sample} sample_common ${TYCam_LIBS} ${COMMON_GUI_LIBS} ${CLOUD_VIEWER})
        set_target_properties(${sample} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/)
        set_target_properties(${sample} PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON )
        install(TARGETS ${sample} RUNTIME DESTINATION samples/)
    endif()
endforeach()

