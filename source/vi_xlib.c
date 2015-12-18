#include "include/wl_def.h"

#include <sys/ipc.h>
#include <sys/shm.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>
#include <X11/Xatom.h>
#include <X11/extensions/XShm.h>


byte *gfxbuf = NULL;
byte *disbuf = NULL;

Display *dpy;
int screen;
Window root, win;
XVisualInfo *vi;
GC gc;
XImage *img;
Colormap cmap;
Atom wmDeleteWindow;

XShmSegmentInfo shminfo;
XColor clr[256];

int indexmode;
int shmmode;

static byte cpal[768];
static word spal[768];
static dword ipal[768];

static int ByteOrder; /* 0 = LSBFirst, 1 = MSBFirst */
static int NeedSwap;

int MyDepth;

void GetVisual()
{
	XVisualInfo vitemp;
		
	int i, numVisuals;
		
	vitemp.screen = screen;
	vitemp.depth = 8;
	vitemp.class = PseudoColor;
	
	vi = XGetVisualInfo(dpy, VisualScreenMask | VisualDepthMask |
			    VisualClassMask, &vitemp, &numVisuals);
	
	if (vi && (numVisuals > 0)) {
		indexmode = 1;
	
		cmap = XCreateColormap(dpy, root, vi->visual, AllocAll);
		for (i = 0; i < 256; i++) {
			clr[i].pixel = i;
			clr[i].flags = DoRed|DoGreen|DoBlue;
		}
		
		return;	
	}
	
	vitemp.depth = 15;
	vitemp.class = TrueColor;
	vitemp.red_mask = 0x7C00;
	vitemp.green_mask = 0x03E0;
	vitemp.blue_mask = 0x001F;
	vi = XGetVisualInfo(dpy, VisualScreenMask | VisualDepthMask |
			    VisualClassMask, &vitemp, &numVisuals);
	
	if (vi && (numVisuals > 0)) {
		indexmode = 0;
		
		cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);
		
		return;
	} 
	
	vitemp.depth = 16;
	vitemp.red_mask = 0xF800;
	vitemp.green_mask = 0x07E0;
	vitemp.blue_mask = 0x001F;
	vi = XGetVisualInfo(dpy, VisualScreenMask | VisualDepthMask |
			    VisualClassMask, &vitemp, &numVisuals);
	
	if (vi && (numVisuals > 0)) {
		indexmode = 0;

		cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);
		
		return;
	}
	
	vitemp.depth = 24;
	vitemp.red_mask = 0xFF0000;
	vitemp.green_mask = 0x00FF00;
	vitemp.blue_mask = 0x0000FF;
	vi = XGetVisualInfo(dpy, VisualScreenMask | VisualDepthMask |
			    VisualClassMask, &vitemp, &numVisuals);
	
	if (vi && (numVisuals > 0)) {
		indexmode = 0;
		
		cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);
		
		return;
	}

	vitemp.depth = 32;
	
	vi = XGetVisualInfo(dpy, VisualScreenMask | VisualDepthMask |
			    VisualClassMask, &vitemp, &numVisuals);
	
	if (vi && (numVisuals > 0)) {
		indexmode = 0;

		cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);
		
		return;
	}

	Quit("No usable visual found!");		
}

int GetBPP()
{
	switch(img->depth) {
		case 4:
			break;
		case 8:
			if (img->bits_per_pixel == 8)
				return 8;
			break;
		case 15:
			if (img->bits_per_pixel == 16)
				return 15;
			break;
		case 16:
			if (img->bits_per_pixel == 16)
				return 16;
			break;
		case 24:
			if (img->bits_per_pixel == 24)
				return 24;
			else
				return 32;
			break;
		case 32:
			if (img->bits_per_pixel == 32)
				return 32;
			break;
	}
	fprintf(stderr, "Unsupported combination of depth %d and bits per pixel %d...\n", img->depth, img->bits_per_pixel);
	fprintf(stderr, "pad = %d, unit = %d, bits = %d, bpl = %d, rgb = %d, depth = %d (%d)\n", img->bitmap_pad, img->bitmap_unit, img->bits_per_pixel, img->bytes_per_line, vi->bits_per_rgb, img->depth, vi->depth);
	exit(EXIT_FAILURE);
}

void VL_Startup()
{
	XSetWindowAttributes attr;
	XSizeHints sizehints;	
	XGCValues gcvalues;
	Pixmap bitmap;
	Cursor cursor;
	XColor bg = { 0 };
	XColor fg = { 0 };
	char data[8] = { 0x01 };
	
	char *disp;
	int attrmask;
	
	disp = getenv("DISPLAY");
	
	dpy = XOpenDisplay(disp);
	
	if (dpy == NULL) {
		fprintf(stderr, "Unable to open display %s!\n", XDisplayName(disp));
		exit(EXIT_FAILURE);
	}
	
	screen = DefaultScreen(dpy);
	
	root = RootWindow(dpy, screen);
	
	GetVisual();
	
	attr.colormap = cmap;		   
	attr.event_mask = KeyPressMask | KeyReleaseMask | ExposureMask 
		| FocusChangeMask | StructureNotifyMask;
	attrmask = CWColormap | CWEventMask;
	
	win = XCreateWindow(dpy, root, 0, 0, 320, 200, 0, CopyFromParent, 
			    InputOutput, vi->visual, attrmask, &attr);
	
	if (win == None) {
		Quit("Unable to create window!");
	}
	
	
	gcvalues.foreground = BlackPixel(dpy, screen);
	gcvalues.background = WhitePixel(dpy, screen);
	gc = XCreateGC(dpy, win, GCForeground | GCBackground, &gcvalues);
	
	sizehints.min_width = 320;
	sizehints.min_height = 200;
	sizehints.max_width = 320;
	sizehints.max_height = 200;
	sizehints.base_width = 320;
	sizehints.base_height = 200;
	sizehints.flags = PMinSize | PMaxSize | PBaseSize;
	
	XSetWMProperties(dpy, win, NULL, NULL, _argv, _argc, &sizehints, None, None); 
	
	XStoreName(dpy, win, GAMENAME);
	XSetIconName(dpy, win, GAMENAME);
	
	wmDeleteWindow = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(dpy, win, &wmDeleteWindow, 1);

	bitmap = XCreateBitmapFromData(dpy, win, data, 8, 8);
	cursor = XCreatePixmapCursor(dpy, bitmap, bitmap, &fg, &bg, 0, 0);
	XDefineCursor(dpy, win, cursor);
	
	shmmode = 0;
	
	if (!MS_CheckParm("noshm") && (XShmQueryExtension(dpy) == True)) {
		img = XShmCreateImage(dpy, vi->visual, vi->depth, ZPixmap, 
				      NULL, &shminfo, 320, 200);

		shminfo.shmid = shmget(IPC_PRIVATE, img->bytes_per_line * img->height, IPC_CREAT | 0777);
		shminfo.shmaddr = img->data = shmat(shminfo.shmid, 0, 0);	
		shminfo.readOnly = False;
		disbuf = (byte *)img->data;
			
		if (indexmode)
			gfxbuf = disbuf;
		else
			gfxbuf = malloc(vwidth * vheight * 1);
			
		if (XShmAttach(dpy, &shminfo) == True) {
			printf("Using XShm Extension...\n");
			shmmode = 1;
			
			shmctl(shminfo.shmid, IPC_RMID, 0);
		} else {
			printf("Error with XShm...\n");
		}
	}
				
	if (img == NULL) {
		XImage *imgtmp;
		char *gb;
		
		printf("Falling back on XImage...\n");
		
		
		gb = (char *)malloc(320);
		imgtmp = XCreateImage(dpy, vi->visual, vi->depth, ZPixmap, 0,
				gb, 16, 1, 8, 16*4);
		
		if (gfxbuf == NULL) 
			gfxbuf = malloc(vwidth * vheight * 1);
		if (indexmode) 
			disbuf = gfxbuf;
		else 
			disbuf = malloc(vwidth * vheight * (imgtmp->bits_per_pixel / 8));
		
		img = XCreateImage(dpy, vi->visual, vi->depth, ZPixmap, 0,
			(char *)disbuf, vwidth, vheight, 8, 0 * (imgtmp->bits_per_pixel / 8));
	
		if (img == NULL) {
			Quit("XCreateImage returned NULL");
		}
		
		XInitImage(img);
		XDestroyImage(imgtmp);
	}
		
	MyDepth = GetBPP();

#if BYTE_ORDER == BIG_ENDIAN
	ByteOrder = 1;
#else
	ByteOrder = 0;
#endif

	NeedSwap = (ByteOrder != img->byte_order);

	XMapRaised(dpy, win);
}

void VL_Shutdown()
{
	if ( !shmmode && (gfxbuf != NULL) ) {
		free(gfxbuf);
		gfxbuf = NULL;
	}
	
	if ( shmmode && !indexmode && (gfxbuf != NULL) ) {
		free(gfxbuf);
		gfxbuf = NULL;
	}
	
	if (shmmode) {
		XShmDetach(dpy, &shminfo);
		XDestroyImage(img);
		shmdt(shminfo.shmaddr);
		shmctl(shminfo.shmid, IPC_RMID, 0);
	} else if ( (indexmode == 0) && (disbuf != NULL) ) {
		free(disbuf);
		disbuf = NULL;
	}
}

void VL_WaitVBL(int vbls)
{
	/* hack - but it works for me */
	long last = get_TimeCount() + vbls;
	while (last > get_TimeCount()) ;
}

void VW_UpdateScreen()
{
	dword *ptri;
	word *ptrs;
	byte *ptrb;
	
	int i;

	if (indexmode == 0) {
		switch(MyDepth) {
		case 15:
		case 16:
			ptrs = (word *)disbuf;
			
			for (i = 0; i < 64000; i++) {
				*ptrs = spal[gfxbuf[i]];
				ptrs++;
			}
			break;
		case 24: /* Endian Safe? Untested. */
			ptrb = disbuf;
			for (i = 0; i < 64000; i++) {
				*ptrb = cpal[gfxbuf[i]*3+2] << 2; ptrb++;
				*ptrb = cpal[gfxbuf[i]*3+1] << 2; ptrb++;
				*ptrb = cpal[gfxbuf[i]*3+0] << 2; ptrb++;
			}
			break;
		case 32:
			ptri = (dword *)disbuf;
			
			for (i = 0; i < 64000; i++) {
				*ptri = ipal[gfxbuf[i]];
				ptri++;
			}
			break;
		default:
			break;
			/* ... */
		}
	}

	if (shmmode)
		XShmPutImage(dpy, win, gc, img, 0, 0, 0, 0, 320, 200, False);
	else
		XPutImage(dpy, win, gc, img, 0, 0, 0, 0, 320, 200);
}

void keyboard_handler(int code, int press);
static int XKeysymToScancode(KeySym keysym);

static void HandleXEvents()
{
	XEvent event;
	
	if (XPending(dpy)) {
		do {
			XNextEvent(dpy, &event);
			switch(event.type) {
				case KeyPress:
					keyboard_handler(XKeysymToScancode(XKeycodeToKeysym(dpy, event.xkey.keycode, 0)), 1);
					break;
				case KeyRelease:
					keyboard_handler(XKeysymToScancode(XKeycodeToKeysym(dpy, event.xkey.keycode, 0)), 0);
					break;
				case Expose:
					VW_UpdateScreen();
					break;
				case ClientMessage:
					if (event.xclient.data.l[0] == wmDeleteWindow)
						Quit(NULL);
					break;
				case ConfigureNotify:
					break;
				case FocusIn:
					break;
				case FocusOut:
					break;
				default:
					break;
			}
		} while (XPending(dpy));
	}
}

/* ======================================================================== */

/*
=================
=
= VL_SetPalette
=
=================
*/

void VL_SetPalette(const byte *palette)
{
	int i;

	VL_WaitVBL(1);
	
	if (indexmode) {
		for (i = 0; i < 256; i++) {
			clr[i].red = palette[i*3+0] << 10;
			clr[i].green = palette[i*3+1] << 10;
			clr[i].blue = palette[i*3+2] << 10;
		}
		XStoreColors(dpy, cmap, clr, 256);
	} else {
		memcpy(cpal, palette, 768);
		for (i = 0; i < 256; i++) {
/* TODO: this should really use the visual for creating the pixel */		
			switch(MyDepth) {
				case 8:
					break;
				case 15: /* Endian Safe? Untested. */
					spal[i] = ((palette[i*3+0] >> 1) << 10) | ((palette[i*3+1] >> 1) << 5) | ((palette[i*3+2] >> 1) << 0);
					if (NeedSwap)
						spal[i] = SwapInt16(spal[i]);
					break;
				case 16:
					spal[i] = ((palette[i*3+0] >> 1) << 11) | ((palette[i*3+1] >> 0) << 5) | ((palette[i*3+2] >> 1) << 0);
					if (NeedSwap)
						spal[i] = SwapInt16(spal[i]);
					break;
				case 32:
					ipal[i] = (palette[i*3+0] << 18) | (palette[i*3+1] << 10) | (palette[i*3+2] << 2);
					if (NeedSwap)
						ipal[i] = SwapInt32(ipal[i]);
					break;
			}
		}
		VW_UpdateScreen();
	}		
}

/*
=================
=
= VL_GetPalette
=
=================
*/

void VL_GetPalette(byte *palette)
{	
	int i;
	
	if (indexmode) {
		for (i = 0; i < 256; i++) {
			palette[i*3+0] = clr[i].red >> 10;
			palette[i*3+1] = clr[i].green >> 10;
			palette[i*3+2] = clr[i].blue >> 10;
		}
	} else {
		memcpy(palette, cpal, 768);
	}
}

int main(int argc, char *argv[])
{
	vwidth = 320;
	vheight = 200;
	                	                
	return WolfMain(argc, argv);
}

/*
==========================
=
= Quit
=
==========================
*/

void DisplayTextSplash(byte *text, int l);

void Quit(char *error)
{
	memptr screen = NULL;
	int l = 0;
	
	if (!error || !*error) {
		CA_CacheGrChunk(ORDERSCREEN);
		screen = grsegs[ORDERSCREEN];
		l = 24;
		WriteConfig();
	} else if (error) {
		CA_CacheGrChunk(ERRORSCREEN);
		screen = grsegs[ERRORSCREEN];
		l = 7;
	}
	
	ShutdownId();
	
	if (screen) {
		DisplayTextSplash(screen, l);
	}
	
	if (error && *error) {
		fprintf(stderr, "Quit: %s\n", error);
		exit(EXIT_FAILURE);
 	}
	exit(EXIT_SUCCESS);
}

static int XKeysymToScancode(KeySym keysym)
{
	switch (keysym) {
		case XK_1:
			return sc_1;
		case XK_2:
			return sc_2;
		case XK_3:
			return sc_3;
		case XK_4:
			return sc_4;
		case XK_5:
			return sc_5;
		case XK_6:
			return sc_6;
		case XK_a:
			return sc_A;
		case XK_b:
			return sc_B;
		case XK_c:
			return sc_C;
		case XK_d:
			return sc_D;
		case XK_g:
			return sc_G;
		case XK_h:
			return sc_H;
		case XK_i:
			return sc_I;
		case XK_l:
			return sc_L;
		case XK_m:
			return sc_M;
		case XK_n:
			return sc_N;
		case XK_q:
			return sc_Q;
		case XK_s:
			return sc_S;
		case XK_t:
			return sc_T;
		case XK_y:
			return sc_Y;
		case XK_F1:
			return sc_F1;
		case XK_F2:
			return sc_F2;
		case XK_F3:
			return sc_F3;
		case XK_F10:
			return sc_F10;
		case XK_F11:
			return sc_F11;
		case XK_F12:
			return sc_F12;
		case XK_Left:
		case XK_KP_Left:
			return sc_LeftArrow;
		case XK_Right:
		case XK_KP_Right:
			return sc_RightArrow;
		case XK_Up:
		case XK_KP_Up:
			return sc_UpArrow;
		case XK_Down:
		case XK_KP_Down:
			return sc_DownArrow;
		case XK_Control_L:
		case XK_Control_R:
			return sc_Control;
		case XK_Alt_L:
		case XK_Alt_R:
			return sc_Alt;
		case XK_Shift_L:
			return sc_LShift;
		case XK_Shift_R:
			return sc_RShift;
		case XK_Escape:
			return sc_Escape;
		case XK_space:
		case XK_KP_Space:
			return sc_Space;
		case XK_KP_Enter:
		case XK_Return:
			return sc_Enter;
		case XK_Tab:
			return sc_Tab;
		case XK_BackSpace:
			return sc_BackSpace;
		case XK_Pause:
			return 0xE1;
		default:
			printf("unknown: %s\n", XKeysymToString(keysym));
			return sc_None;
	}
}

void INL_Update()
{
	HandleXEvents();
}
