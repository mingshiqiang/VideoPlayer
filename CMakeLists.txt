cmake_minimum_required(VERSION 3.19)
project(VideoPlayer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Qt6 modules
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Multimedia OpenGL OpenGLWidgets Network Svg)

qt_standard_project_setup()

# Qt resources (SVG icons)
qt_add_resources(RESOURCES resources.qrc)

# FFmpeg paths
set(FFMPEG_ROOT "${CMAKE_SOURCE_DIR}/third_part/ffmpeg-8.1-full_build-shared")
set(FFMPEG_INCLUDE_DIR "${FFMPEG_ROOT}/include")
set(FFMPEG_LIB_DIR "${FFMPEG_ROOT}/lib")
set(FFMPEG_BIN_DIR "${FFMPEG_ROOT}/bin")

# FFmpeg libraries
set(FFMPEG_LIBS
    avcodec
    avformat
    avutil
    swscale
    swresample
    avdevice
)

# Source files
set(PROJECT_SOURCES
    main.cpp
    mainwindow.h
    mainwindow.cpp
    core/ffmpeg_player.h
    core/ffmpeg_player.cpp
    core/audio_output.h
    core/audio_output.cpp
    core/video_frame_queue.h
    core/dbg_trace.h
    ui/titlebar.h
    ui/titlebar.cpp
    ui/controlbar.h
    ui/controlbar.cpp
    ui/video_canvas.h
    ui/video_canvas.cpp
    ui/slider.h
    ui/slider.cpp
    ui/playlist_widget.h
    ui/playlist_widget.cpp
    ui/settings_dialog.h
    ui/settings_dialog.cpp
)

qt_add_executable(VideoPlayer
    WIN32
    ${PROJECT_SOURCES}
    ${RESOURCES}
)

# Include directories
target_include_directories(VideoPlayer PRIVATE
    ${FFMPEG_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}
)

# Qt libraries
target_link_libraries(VideoPlayer
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Multimedia
        Qt::OpenGL
        Qt::OpenGLWidgets
        Qt::Network
        Qt::Svg
)

# FFmpeg libraries
foreach(lib ${FFMPEG_LIBS})
    target_link_libraries(VideoPlayer PRIVATE "${FFMPEG_LIB_DIR}/${lib}.lib")
endforeach()

# Windows: copy FFmpeg DLLs to output directory
if(WIN32)
    add_custom_command(TARGET VideoPlayer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${FFMPEG_BIN_DIR}
        $<TARGET_FILE_DIR:VideoPlayer>
        COMMENT "Copying FFmpeg DLLs to output directory"
    )
endif()

include(GNUInstallDirs)

install(TARGETS VideoPlayer
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET VideoPlayer
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
