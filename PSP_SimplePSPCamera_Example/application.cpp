#include "Umfeld.h"
#include "SimplePSPCamera.h"

using namespace umfeld;

SimplePSPCamera* pspCamera;
u32 *framebuffer;
PImage* umfeldImg;

//int mWW = 128;  // POT for 160x120
//int mHH = 128;

//int mWW = 256;  // POT for 320x240
//int mHH = 256;

int mWW = 512;  // POT for 352x288
int mHH = 512;


// Helper to scale  camera data to  display buffer
void copyAndScaleCameraToFramebuffer(u32* src, int src_width, int src_height,
                                      u32* dst, int dst_width, int dst_height) {
    for (int y = 0; y < dst_height; y++) {
        for (int x = 0; x < dst_width; x++) {
            int src_x = (x * src_width) / dst_width;
            int src_y = (y * src_height) / dst_height;

            if (src_x < src_width && src_y < src_height) {
                u32 pixel = src[src_y * src_width + src_x];
                pixel = (pixel & 0x00FFFFFF) | 0xFF000000;
                dst[y * dst_width + x] = pixel;
            } else {
                dst[y * dst_width + x] = color_pack_i(0, 0, 0, 255);
            }
        }
    }
}

void settings() {
    size(480, 272, RENDERER_SDL_2D);
}

void setup() {
    // Initialize camera
    pspCamera = new SimplePSPCamera();
    pspCamera->init();

    // Create display framebuffer (POT-safe)
    framebuffer = new u32[mWW * mHH];
    umfeldImg = new PImage();
    umfeldImg->init(framebuffer, mWW, mHH);

    // Initialize with black while waiting for camera
    for (int i = 0; i < mWW * mHH; i++) {
        framebuffer[i] = color_pack_i(0, 0, 0, 255);
    }
}

void draw() {
    background(216);

    umfeld::fill(255, 255, 0);
    noStroke();
    ellipse(width/2, height/2, 50, 50);

    // Read camera frame and scale to POT
    if (pspCamera->isFrameReady()) {
        // Copy and scale camera data to framebuffer
        copyAndScaleCameraToFramebuffer(
            pspCamera->framebuffer, CAM_WIDTH, CAM_HEIGHT,
            framebuffer, mWW, mHH
        );

        // Update texture for rendering
        umfeldImg->updatePixels();

        // Signal ready for next frame
        pspCamera->nextFrame();
    }

    umfeld::fill(255);
    image(umfeldImg, 0, 0, umfeldImg->width, umfeldImg->height);
}
