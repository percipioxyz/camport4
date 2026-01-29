if(${CMAKE_VERSION} VERSION_LESS "3.5")
    cmake_minimum_required(VERSION 2.8)
else() # to avoid errors reported by CMake 4.0 and above versions
    cmake_minimum_required(VERSION 3.5)
endif()

if(NOT BUILD_SAMPLE_WITH_GUI)
    message(STATUS "Build Samples without GUI")
    set(BUILD_SAMPLE_V2_WITH_OPENCV OFF PARENT_SCOPE)
    set(COMMON_GUI_SRC ${CMAKE_CURRENT_SOURCE_DIR}/TyDummyWindow.cpp PARENT_SCOPE)
else()
    message(STATUS "Build Samples with GUI")
    if (NOT OpenCV_FOUND)
        set(BUILD_SAMPLE_V2_WITH_OPENCV OFF PARENT_SCOPE)
        message(WARNING "OpenCV library not found, try Use CImg")
        #Only check X11 under Unix. Suppose All Windows system must have GDI32
        if(UNIX)
            find_package(X11)
            if(X11_FOUND)
                file(REMOVE_RECURSE ${CMAKE_CURRENT_SOURCE_DIR}/CImg)
                execute_process(
                    COMMAND unzip CImg.zip
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                )
                message(STATUS "Find X11, Use X11")
                set(COMMON_GUI_INC ${CMAKE_CURRENT_SOURCE_DIR}/CImg PARENT_SCOPE)
                set(COMMON_GUI_SRC ${CMAKE_CURRENT_SOURCE_DIR}/TyDisplayWindow.cpp PARENT_SCOPE)
                set(COMMON_GUI_LIBS X11 PARENT_SCOPE)
            else()
                message(WARNING "X11 not found, try build sample without GUI")
                set(COMMON_GUI_SRC ${CMAKE_CURRENT_SOURCE_DIR}/TyDummyWindow.cpp PARENT_SCOPE)
            endif()
        else()
            file(REMOVE_RECURSE ${CMAKE_CURRENT_SOURCE_DIR}/CImg)
            execute_process(
                COMMAND powershell -Command "Expand-Archive -Path CImg.zip -DestinationPath ."
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            )
            message(STATUS "Windows OS, Use gdi32")
            set(COMMON_GUI_INC ${CMAKE_CURRENT_SOURCE_DIR}/CImg PARENT_SCOPE)
            set(COMMON_GUI_SRC ${CMAKE_CURRENT_SOURCE_DIR}/TyDisplayWindow.cpp PARENT_SCOPE)
            set(COMMON_GUI_LIBS gdi32 PARENT_SCOPE)
        endif()
    else()
        set(COMMON_GUI_INC ${CMAKE_CURRENT_SOURCE_DIR}/ PARENT_SCOPE)
        set(COMMON_GUI_SRC ${CMAKE_CURRENT_SOURCE_DIR}/MatViewer.cpp ${CMAKE_CURRENT_SOURCE_DIR}/TyOpenCvWindow.cpp PARENT_SCOPE)
    endif()
    
    include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
    
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/CImg)
    include_directories(${OPENGL_INCLUDE_DIRS})
endif()
