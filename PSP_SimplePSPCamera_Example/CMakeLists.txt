cmake_minimum_required(VERSION 3.12)

project(PSP_SimplePSPCamera_Example)
set(UMFELD_PATH $ENV{UMFELD_PATH})


# --------- no need to change anything below this line ------------
if (PSP)
    option(DISABLE_VIDEO "Disable video ( ffmpeg ) support" ON)
    option(DISABLE_PDF "Disable PDF export ( cairo )" ON)
    option(DISABLE_NCURSES "Disable ncurses support" ON)
    option(DISABLE_OPENGL "Disable OpenGL support" ON)
    option(DISABLE_MIDI "Disable MIDI support" ON)
    option(DISABLE_SERIAL "Disable Serial/UART support" ON)
    option(DISABLE_NATIVE_FILE_DIALOGS "Disable tiny file dialogs support" ON)
    option(ENABLE_PORTAUDIO "Enable PortAudio output" OFF)
    option(ENABLE_CAPTURE "Enable Capture" OFF)
    option(ENABLE_ANGLE "Enable ANGLE OpenGL ES emulation on macOS ( apple silicon )" OFF)
    add_compile_options(-fno-pic -fno-pie)
    add_link_options(-fno-pie -no-pie)
endif (PSP)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(".")

# Only compile application.cpp and SimplePSPCamera.cpp - no UHT, no utilities
add_executable(${PROJECT_NAME}
        application.cpp
        ../PSP_SimplePSPCamera/SimplePSPCamera.cpp
)

include_directories(
        ../PSP_SimplePSPCamera
)

add_subdirectory(${UMFELD_PATH} ${CMAKE_BINARY_DIR}/umfeld-lib-${PROJECT_NAME})
add_umfeld_libs()

# --- add psp library ---------------------------------------------

if (PSP)
    target_link_libraries(${PROJECT_NAME} PRIVATE
            pspgum_vfpu
            pspgu
            pspdebug
            pspdisplay
            pspge
            pspaudio
            pspaudiolib
            pspusb
            pspusbcam
            pspjpeg
            pspvfpu
    )
    string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%d %H:%M")
    create_pbp_file(
            TARGET ${PROJECT_NAME}
            ICON_PATH NULL
            BACKGROUND_PATH NULL
            PREVIEW_PATH NULL
            TITLE "${PROJECT_NAME} ${BUILD_TIMESTAMP}"
            VERSION 01.00
    )
endif (PSP)
