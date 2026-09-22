// macos
#include <stdio.h>
#include <stdlib.h>
// #include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "msettings.h"

#include "defines.h"
#include "platform.h"
#include "api.h"
#include "utils.h"

#include "scaler.h"

void InitSettings(void){}
void QuitSettings(void){}

int GetBrightness(void) { return 0; }
int GetVolume(void) { return 0; }

void SetRawBrightness(int value) {}
void SetRawVolume(int value){}

void SetBrightness(int value) {}
void SetVolume(int value) {}

int GetJack(void) { return 0; }
void SetJack(int value) {}

int GetHDMI(void) { return 0; }
void SetHDMI(int value) {}

int GetMute(void) { return 0; }

///////////////////////////////

static SDL_Joystick *joystick;
void PLAT_initInput(void) {
	SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	joystick = SDL_JoystickOpen(0);
}
void PLAT_quitInput(void) {
	SDL_JoystickClose(joystick);
	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

///////////////////////////////

static struct VID_Context {
	SDL_Window* window;
	SDL_Renderer* renderer;
	SDL_Texture* texture;
	SDL_Surface* buffer;
	SDL_Surface* screen;

	GFX_Renderer* blit; // yeesh

	int width;
	int height;
	int pitch;
} vid;

static int device_width;
static int device_height;
static int device_pitch;
static int rotate = 0;

// The emulated device geometry. FIXED_WIDTH, FIXED_HEIGHT, FIXED_SCALE and
// PADDING in platform.h read these, so one binary can stand in for any panel.
// The defaults are the 640x480 device this build has always emulated.
int macos_screen_width = 640;
int macos_screen_height = 480;
int macos_screen_scale = 2;
int macos_screen_padding = 10;

// readScreenEnv overrides one geometry value from the environment, ignoring
// anything that is not a positive integer.
static void readScreenEnv(const char* name, int* out) {
	const char* value = getenv(name);
	if (value==NULL || *value=='\0') return;

	char* end = NULL;
	long parsed = strtol(value, &end, 10);
	if (end==value || *end!='\0' || parsed<=0) return;

	*out = (int)parsed;
}

// initScreenGeometry reads the emulated panel from MINUI_WIDTH, MINUI_HEIGHT,
// MINUI_SCALE and MINUI_PADDING. Call it before anything reads FIXED_*.
static void initScreenGeometry(void) {
	readScreenEnv("MINUI_WIDTH", &macos_screen_width);
	readScreenEnv("MINUI_HEIGHT", &macos_screen_height);
	readScreenEnv("MINUI_SCALE", &macos_screen_scale);
	readScreenEnv("MINUI_PADDING", &macos_screen_padding);
}

// macOS window width; the height follows the emulated panel's aspect ratio
#define WINDOW_WIDTH 800
#define CONTENT_ASPECT_RATIO ((double)FIXED_WIDTH / (double)FIXED_HEIGHT)

// Calculate centered destination rectangle for current window size
static SDL_Rect getContentRect(void) {
	int win_w, win_h;
	SDL_GetWindowSize(vid.window, &win_w, &win_h);

	int dest_w, dest_h;
	if ((double)win_w / win_h > CONTENT_ASPECT_RATIO) {
		// Window is wider than the panel - pillarbox (black bars on sides)
		dest_h = win_h;
		dest_w = (int)(win_h * CONTENT_ASPECT_RATIO);
	} else {
		// Window is taller than the panel - letterbox (black bars top/bottom)
		dest_w = win_w;
		dest_h = (int)(win_w / CONTENT_ASPECT_RATIO);
	}

	SDL_Rect rect;
	rect.x = (win_w - dest_w) / 2;
	rect.y = (win_h - dest_h) / 2;
	rect.w = dest_w;
	rect.h = dest_h;
	return rect;
}

// macOS event filter to handle Cmd+Q (SDL_QUIT) and window resize events
static int macOS_eventFilter(void* userdata, SDL_Event* event) {
	if (event->type == SDL_QUIT) {
		exit(0);
	}
	// Handle window resize/fullscreen toggle to force redraw
	if (event->type == SDL_WINDOWEVENT) {
		if (event->window.event == SDL_WINDOWEVENT_RESIZED ||
		    event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
		    event->window.event == SDL_WINDOWEVENT_EXPOSED) {
			// Force a redraw with the current texture content
			if (vid.renderer && vid.texture) {
				SDL_SetRenderDrawColor(vid.renderer, 0, 0, 0, 255);
				SDL_RenderClear(vid.renderer);
				SDL_Rect content_rect = getContentRect();
				SDL_RenderCopy(vid.renderer, vid.texture, NULL, &content_rect);
				SDL_RenderPresent(vid.renderer);
			}
		}
	}
	return 1; // allow all other events
}

SDL_Surface* PLAT_initVideo(void) {
	initScreenGeometry();

	SDL_InitSubSystem(SDL_INIT_VIDEO);
	SDL_ShowCursor(0);

	int w = FIXED_WIDTH;
	int h = FIXED_HEIGHT;
	int p = FIXED_PITCH;
	int window_h = (int)(WINDOW_WIDTH / CONTENT_ASPECT_RATIO);
	vid.window   = SDL_CreateWindow("minui-keyboard", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH, window_h, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
	vid.renderer = SDL_CreateRenderer(vid.window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);

	// SDL_RendererInfo info;
	// SDL_GetRendererInfo(vid.renderer, &info);
	// LOG_info("Current render driver: %s\n", info.name);

	vid.texture = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w,h);

	// SDL_SetTextureScaleMode(vid.texture, SDL_ScaleModeNearest);

	vid.buffer	= SDL_CreateRGBSurfaceFrom(NULL, w,h, FIXED_DEPTH, p, RGBA_MASK_565);
	vid.screen	= SDL_CreateRGBSurface(SDL_SWSURFACE, w,h, FIXED_DEPTH, RGBA_MASK_565);
	vid.width	= w;
	vid.height	= h;
	vid.pitch	= p;

	PWR_disablePowerOff();

	device_width	= w;
	device_height	= h;
	device_pitch	= p;

	// Add event filter to handle Cmd+Q and window resize on macOS
	SDL_AddEventWatch(macOS_eventFilter, NULL);

	return vid.screen;
}

static void clearVideo(void) {
	for (int i=0; i<3; i++) {
		SDL_RenderClear(vid.renderer);
		SDL_FillRect(vid.screen, NULL, 0);

		SDL_LockTexture(vid.texture,NULL,&vid.buffer->pixels,&vid.buffer->pitch);
		SDL_FillRect(vid.buffer, NULL, 0);
		SDL_UnlockTexture(vid.texture);
		SDL_RenderCopy(vid.renderer, vid.texture, NULL, NULL);

		SDL_RenderPresent(vid.renderer);
	}
}

void PLAT_quitVideo(void) {
	clearVideo();

	SDL_FreeSurface(vid.screen);
	SDL_FreeSurface(vid.buffer);
	SDL_DestroyTexture(vid.texture);
	SDL_DestroyRenderer(vid.renderer);
	SDL_DestroyWindow(vid.window);

	SDL_Quit();
}

void PLAT_clearVideo(SDL_Surface* screen) {
	SDL_FillRect(screen, NULL, 0); // TODO: revisit
}
void PLAT_clearAll(void) {
	PLAT_clearVideo(vid.screen); // TODO: revist
	SDL_RenderClear(vid.renderer);
}

void PLAT_setVsync(int vsync) {

}

static void resizeVideo(int w, int h, int p) {
	if (w==vid.width && h==vid.height && p==vid.pitch) return;

	LOG_info("resizeVideo(%i,%i,%i)\n",w,h,p);

	SDL_FreeSurface(vid.buffer);
	SDL_DestroyTexture(vid.texture);
	// PLAT_clearVideo(vid.screen);

	vid.texture = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w,h);
	// SDL_SetTextureScaleMode(vid.texture, SDL_ScaleModeNearest);

	vid.buffer	= SDL_CreateRGBSurfaceFrom(NULL, w,h, FIXED_DEPTH, p, RGBA_MASK_565);

	vid.width	= w;
	vid.height	= h;
	vid.pitch	= p;
}

SDL_Surface* PLAT_resizeVideo(int w, int h, int p) {
	resizeVideo(w,h,p);
	return vid.screen;
}

void PLAT_setVideoScaleClip(int x, int y, int width, int height) {

}
void PLAT_setNearestNeighbor(int enabled) {
	// always enabled?
}
void PLAT_setSharpness(int sharpness) {
	// buh
}
void PLAT_vsync(int remaining) {
	if (remaining>0) SDL_Delay(remaining);
}

scaler_t PLAT_getScaler(GFX_Renderer* renderer) {
	return scale1x1_c16;
}

void PLAT_blitRenderer(GFX_Renderer* renderer) {
	vid.blit = renderer;
	SDL_RenderClear(vid.renderer);
	resizeVideo(vid.blit->true_w,vid.blit->true_h,vid.blit->src_p);
	scale1x1_c16(
		renderer->src,renderer->dst,
		renderer->true_w,renderer->true_h,renderer->src_p,
		vid.screen->w,vid.screen->h,vid.screen->pitch // fixed in this implementation
		// renderer->dst_w,renderer->dst_h,renderer->dst_p
	);
}

// saveScreenshot writes the frame at the emulated panel's own resolution to the
// path in MINUI_SCREENSHOT, overwriting it each flip. That is how the layout is
// checked and the README screenshots are regenerated at a device geometry the
// developer does not have to hand; the window itself is scaled for the desktop.
static void saveScreenshot(void) {
	const char* path = getenv("MINUI_SCREENSHOT");
	if (path==NULL || *path=='\0') return;

	SDL_SaveBMP(vid.screen, path);
}

void PLAT_flip(SDL_Surface* IGNORED, int ignored) {
	saveScreenshot();

	// Clear to black for letterbox/pillarbox bars
	SDL_SetRenderDrawColor(vid.renderer, 0, 0, 0, 255);
	SDL_RenderClear(vid.renderer);

	// Get the centered destination rectangle
	SDL_Rect content_rect = getContentRect();

	if (!vid.blit) {
		resizeVideo(device_width,device_height,FIXED_PITCH);
		SDL_UpdateTexture(vid.texture,NULL,vid.screen->pixels,vid.screen->pitch);
		SDL_RenderCopy(vid.renderer, vid.texture, NULL, &content_rect);
		SDL_RenderPresent(vid.renderer);
		return;
	}

	SDL_LockTexture(vid.texture,NULL,&vid.buffer->pixels,&vid.buffer->pitch);
	SDL_BlitSurface(vid.screen, NULL, vid.buffer, NULL);
	SDL_UnlockTexture(vid.texture);

	SDL_RenderCopy(vid.renderer, vid.texture, NULL, &content_rect);
	SDL_RenderPresent(vid.renderer);
	vid.blit = NULL;
}

///////////////////////////////

// TODO:
#define OVERLAY_WIDTH PILL_SIZE // unscaled
#define OVERLAY_HEIGHT PILL_SIZE // unscaled
#define OVERLAY_BPP 4
#define OVERLAY_DEPTH 16
#define OVERLAY_PITCH (OVERLAY_WIDTH * OVERLAY_BPP) // unscaled
#define OVERLAY_RGBA_MASK 0x00ff0000,0x0000ff00,0x000000ff,0xff000000 // ARGB
static struct OVL_Context {
	SDL_Surface* overlay;
} ovl;

SDL_Surface* PLAT_initOverlay(void) {
	ovl.overlay = SDL_CreateRGBSurface(SDL_SWSURFACE, SCALE2(OVERLAY_WIDTH,OVERLAY_HEIGHT),OVERLAY_DEPTH,OVERLAY_RGBA_MASK);
	return ovl.overlay;
}
void PLAT_quitOverlay(void) {
	if (ovl.overlay) SDL_FreeSurface(ovl.overlay);
}
void PLAT_enableOverlay(int enable) {

}

///////////////////////////////

static int online = 1;
void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	*is_charging = 1;
	*charge = 100;
	return;
}

void PLAT_enableBacklight(int enable) {
	// buh
}

void PLAT_powerOff(void) {
	SND_quit();
	VIB_quit();
	PWR_quit();
	GFX_quit();
	exit(0);
}

///////////////////////////////

void PLAT_setCPUSpeed(int speed) {
	// buh
}

void PLAT_setRumble(int strength) {
	// buh
}

int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

char* PLAT_getModel(void) {
	return "macOS";
}

int PLAT_isOnline(void) {
	return online;
}
