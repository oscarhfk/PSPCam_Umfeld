#pragma once

#include <cstdint>
#include <psppower.h>
#include <psputility_usbmodules.h>
#include <psputility_avmodules.h>
#include <pspusb.h>
#include <pspusbacc.h>
#include <pspusbcam.h>
#include <pspjpeg.h>
#include <pspkernel.h>


//#define CAM_WIDTH 160
//#define CAM_HEIGHT 120
//#define CAM_MAX_FRAME_SIZE (1024*34)  // Back to original

 //#define CAM_WIDTH 320
 //#define CAM_HEIGHT 240
 //#define CAM_MAX_FRAME_SIZE (1024*128)

#define CAM_WIDTH 352
#define CAM_HEIGHT 288
#define CAM_MAX_FRAME_SIZE (1024*128)



class SimplePSPCamera {
public:
    uint32_t* framebuffer;

    SimplePSPCamera();
    ~SimplePSPCamera();

    int init();
    bool isFrameReady();
    void nextFrame();

private:
    int thread_id;
    int semaphore_id;
    bool running;
    bool frame_ready;
    bool app_acked;

    uint8_t* jpeg_buffer;
    uint8_t* work_buffer;

    static int cameraThreadEntry(SceSize args, void* argp);
    int cameraThread(SceSize args, void* argp);

    void loadModules();
    void unloadModules();
    void startUsb();
    void stopUsb();
    int initJpegDecoder();
    int finishJpegDecoder();
};
