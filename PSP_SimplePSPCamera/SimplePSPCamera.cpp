#include "SimplePSPCamera.h"
#include <cstring>
#include <cstdlib>
#include <malloc.h>

static SimplePSPCamera* g_camera = nullptr;

SimplePSPCamera::SimplePSPCamera()
    : framebuffer(nullptr), thread_id(-1), semaphore_id(-1), running(false),
      frame_ready(false), app_acked(false), jpeg_buffer(nullptr), work_buffer(nullptr) {
    g_camera = this;
}

SimplePSPCamera::~SimplePSPCamera() {
    if (thread_id >= 0) {
        sceKernelTerminateDeleteThread(thread_id);
    }
    if (semaphore_id >= 0) {
        sceKernelDeleteSema(semaphore_id);
    }
    finishJpegDecoder();
    stopUsb();
    unloadModules();

    // Don't free static buffers
    framebuffer = nullptr;
    jpeg_buffer = nullptr;
    work_buffer = nullptr;
}

void SimplePSPCamera::loadModules() {
    sceUtilityLoadUsbModule(PSP_USB_MODULE_ACC);
    sceUtilityLoadUsbModule(PSP_USB_MODULE_CAM);
    sceUtilityLoadAvModule(PSP_AV_MODULE_AVCODEC);
}

void SimplePSPCamera::unloadModules() {
    sceUtilityUnloadUsbModule(PSP_USB_MODULE_CAM);
    sceUtilityUnloadUsbModule(PSP_USB_MODULE_ACC);
    sceUtilityUnloadAvModule(PSP_AV_MODULE_AVCODEC);
}

void SimplePSPCamera::startUsb() {
    sceUsbStart(PSP_USBBUS_DRIVERNAME, 0, 0);
    sceUsbStart(PSP_USBACC_DRIVERNAME, 0, 0);
    sceUsbStart(PSP_USBCAM_DRIVERNAME, 0, 0);
    sceUsbStart(PSP_USBCAMMIC_DRIVERNAME, 0, 0);
}

void SimplePSPCamera::stopUsb() {
    sceUsbStop(PSP_USBCAMMIC_DRIVERNAME, 0, 0);
    sceUsbStop(PSP_USBCAM_DRIVERNAME, 0, 0);
    sceUsbStop(PSP_USBACC_DRIVERNAME, 0, 0);
}

int SimplePSPCamera::initJpegDecoder() {
    int result = sceJpegInitMJpeg();
    if (result < 0) return result;

    // PSP JPEG decoder requires height to be padded to 128
    int decoder_height = CAM_HEIGHT;
    if (CAM_HEIGHT % 128 != 0) {
        decoder_height = ((CAM_HEIGHT + 127) / 128) * 128;  // Round up to next 128
    }

    result = sceJpegCreateMJpeg(CAM_WIDTH, decoder_height);
    return result;
}


int SimplePSPCamera::finishJpegDecoder() {
    int result = sceJpegDeleteMJpeg();
    if (result < 0) return result;
    result = sceJpegFinishMJpeg();
    return result;
}

int SimplePSPCamera::cameraThreadEntry(SceSize args, void* argp) {
    if (g_camera) {
        return g_camera->cameraThread(args, argp);
    }
    return 0;
}

int SimplePSPCamera::cameraThread(SceSize args, void* argp) {
    PspUsbCamSetupVideoParam videoparam;
    memset(&videoparam, 0, sizeof(videoparam));
    videoparam.size = sizeof(videoparam);

    // 160x120 - PSP USB camera standard resolution
     videoparam.resolution = PSP_USBCAM_RESOLUTION_160_120;

    // 320x240
    //videoparam.resolution = PSP_USBCAM_RESOLUTION_320_240;

    // // 352x288
    // videoparam.resolution = PSP_USBCAM_RESOLUTION_352_288;




    videoparam.framerate = PSP_USBCAM_FRAMERATE_30_FPS;
    videoparam.saturation = 252;
    videoparam.brightness = 138;
    videoparam.contrast = 62;
    videoparam.framesize = CAM_MAX_FRAME_SIZE;
    videoparam.unk = 1;
    videoparam.evlevel = PSP_USBCAM_EVLEVEL_0_0;

    int result = 0;

    while (1) {
        sceKernelDelayThread(10);

        // Wait for USB connection
        while ((sceUsbGetState() & 0xF) != PSP_USB_CONNECTION_ESTABLISHED) {
            sceKernelDelayThread(1000);
        }

        result = sceUsbCamSetupVideo(&videoparam, work_buffer, CAM_MAX_FRAME_SIZE);
        if (result < 0) {
            running = false;
            continue;
        }

        sceUsbCamAutoImageReverseSW(1);
        result = sceUsbCamStartVideo();
        if (result < 0) {
            running = false;
            continue;
        }

        // Main capture loop
        while (1) {
            uint8_t out_of_usb = 0;

            // Read JPEG frame from camera
            while ((result = sceUsbCamReadVideoFrameBlocking(jpeg_buffer, CAM_MAX_FRAME_SIZE)) < 0) {
                if (++out_of_usb > 10) {
                    goto out_of_usb;
                }
            }

            sceKernelDcacheWritebackAll();

            // Decode JPEG to RGB framebuffer
            sceJpegDecodeMJpeg(jpeg_buffer, result, (uint8_t*)framebuffer, 0);

            sceKernelDelayThread(3);
            scePowerTick(PSP_POWER_TICK_DISPLAY);

            running = true;
            frame_ready = true;

            // Wait for app to acknowledge frame
            sceKernelWaitSema(semaphore_id, 1, 0);
            app_acked = false;
            frame_ready = false;
        }

        out_of_usb:
            sceUsbCamStopVideo();
    }

    return 0;
}

int SimplePSPCamera::init() {
    // Allocate buffers large enough for the configured resolution
    // For 352x288: 352*288*4 = 405,504 bytes
    static uint8_t aligned_framebuffer[352*288*4] __attribute__((aligned(64)));
    static uint8_t aligned_jpeg_buffer[1024*128] __attribute__((aligned(64)));
    static uint8_t aligned_work_buffer[1024*128] __attribute__((aligned(64)));

    framebuffer = reinterpret_cast<uint32_t*>(aligned_framebuffer);
    jpeg_buffer = aligned_jpeg_buffer;
    work_buffer = aligned_work_buffer;

    // Initialize with black
    memset(framebuffer, 0, CAM_WIDTH * CAM_HEIGHT * sizeof(uint32_t));

    // Create semaphore for synchronization
    semaphore_id = sceKernelCreateSema("CameraSema", 0, 2, 2, 0);
    if (semaphore_id < 0) {
        return 0;
    }

    // Load modules and start USB
    loadModules();
    startUsb();

    // Activate camera
    if (sceUsbActivate(PSP_USBCAM_PID) < 0) {
        return 0;
    }

    // Initialize JPEG decoder
    if (initJpegDecoder() < 0) {
        return 0;
    }

    // Start camera thread
    thread_id = sceKernelCreateThread("camera_thread", cameraThreadEntry, 0x10, 0x400*32, 0, NULL);
    if (thread_id < 0) {
        return 0;
    }

    if (sceKernelStartThread(thread_id, 0, NULL) < 0) {
        return 0;
    }

    return 1;
}

bool SimplePSPCamera::isFrameReady() {
    if (frame_ready && !app_acked && running) {
        app_acked = true;
        return true;
    }
    return false;
}

void SimplePSPCamera::nextFrame() {
    sceKernelSignalSema(semaphore_id, 1);
}
