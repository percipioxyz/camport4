
set(ALL_CPP_API_SAMPLES
    ListDevices
    ForceDeviceIP
    Registration
    DepthStream
    TofDepthStream
    SoftTrigger
    ExposureTimeSetting
    ResolutionSetting
    OfflineReconnection
    MultiDeviceOfflineReconnection
    OpenWithIP
    OpenWithInterface
    NetStatistic
    SaveLoadConfig
    GetCalibData
    PointCloud
    StreamAsync
    TofIrStream
    FpsSetting
    )

set(SAMPLES_DEPENDS_OPENCV
    IREnhance
    )

if (BUILD_SAMPLE_V2_WITH_OPENCV)
    set(ALL_CPP_API_SAMPLES ${ALL_CPP_API_SAMPLES} ${SAMPLES_DEPENDS_OPENCV})
endif()

foreach(sample ${ALL_CPP_API_SAMPLES})
    if (DEFINED SAMPLES)
      list(FIND SAMPLES ${sample} index)
      if(index EQUAL -1)
        continue()
      endif()
    endif()
    set(sample_exec ${sample}_v2)
    get_filename_component(spath "${sample}" ABSOLUTE )
    if (EXISTS "${spath}/")
        file(GLOB sources ${sample}/*.cpp)
        
        add_executable(${sample_exec} ${sources})
        add_dependencies(${sample_exec} cpp_api_lib ${TARGET_LIB})
        target_link_libraries(${sample_exec} cpp_api_lib ${TYCam_LIBS} ${COMMON_GUI_LIBS})
        if (BUILD_SAMPLE_V2_WITH_OPENCV)
            target_link_libraries(${sample_exec} ${OpenCV_LIBS})
        endif()
        set_target_properties(${sample_exec} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
        set_target_properties(${sample_exec} PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON )
        install(TARGETS ${sample_exec} RUNTIME DESTINATION samples/)
    endif()
endforeach()
