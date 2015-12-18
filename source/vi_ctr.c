#include "include/wl_def.h"
#include <3ds.h>

byte *gfxbuf = NULL;
static unsigned char pal[768];
u16* fb;

extern void keyboard_handler(int code, int press);
extern boolean InternalKeyboard[NumCodes];

u32 pad;
u16 d_8to16table[256];
vwidth = 400;
vheight = 240;
vstride = 400;

int main (int argc, char *argv[])
{
	aptInit();
	aptOpenSession();
	Result ret=APT_SetAppCpuTimeLimit(30);
	aptCloseSession();
	fsInit();
	gfxInit(GSP_RGB565_OES,GSP_RGB565_OES,false);
	hidInit();
	gfxSetDoubleBuffering(GFX_TOP, false);
	gfxSetDoubleBuffering(GFX_BOTTOM, false);
	gfxSet3D(false);
	consoleInit(GFX_BOTTOM, NULL);
	return WolfMain(argc, argv);
}

void DisplayTextSplash(byte *text);

/*
==========================
=
= Quit
=
==========================
*/

void Quit(char *error)
{
	memptr screen = NULL;

	if (!error || !*error) {
		CA_CacheGrChunk(ORDERSCREEN);
		screen = grsegs[ORDERSCREEN];
		WriteConfig();
	} else if (error) {
		CA_CacheGrChunk(ERRORSCREEN);
		screen = grsegs[ERRORSCREEN];
	}

	ShutdownId();

	if (screen) {
		//DisplayTextSplash(screen);
	}
	
	if (error && *error) {
		printf("Fatal Error: %s\nPress A to exit\n\n", error);
		do{hidScanInput();}while(!(hidKeysDown() & KEY_A));
		gfxExit();
		exit(EXIT_FAILURE);
 	}
	gfxExit();
	exit(EXIT_SUCCESS);
}

void VL_WaitVBL(int vbls)
{
	long last = get_TimeCount() + vbls;
	while (last > get_TimeCount()) ;
}

void VW_UpdateScreen()
{
	int x,y;
	for(x=0; x<vwidth; x++){
		for(y=0; y<vheight;y++){
			fb[((x*240) + (239 - y))] = d_8to16table[gfxbuf[y*vwidth + x]];
		}
	}
}

/*
=======================
=
= VL_Startup
=
=======================
*/

void VL_Startup()
{
	printf("Starting rendering...");
	gfxbuf = malloc(vwidth * vheight);
	fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
	printf("Done!\n");
}

/*
=======================
=
= VL_Shutdown
=
=======================
*/

void VL_Shutdown()
{
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
	VL_WaitVBL(1);
	memcpy(pal, palette, 768);
	int i;
	u8 *pal = palette;
	u16 *table = d_8to16table;
	unsigned r, g, b;
	for(i=0; i<256; i++){
		r = pal[0];
		g = pal[1];
		b = pal[2];
		table[0] = RGB8_to_565(r,g,b);
		table++;
		pal += 3;
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
	memcpy(palette, pal, 768);
}

/*
=================
=
= INL_Update
=
=================
*/
static int mx = 0;
static int my = 0;

void INL_Update()
{
	static u32 buttons = 0;
	static int multiplex = 0;
	u32 new_buttons;
	static int mu = 0;
	static int md = 0;
	
	hidScanInput();
	pad = hidKeysHeld();
	multiplex ^= 1;
	if (multiplex && (pad & (KEY_R | KEY_L)))
		pad &= ~(KEY_DRIGHT | KEY_DLEFT);
	if (!multiplex && (pad & (KEY_DRIGHT | KEY_DLEFT)))
		pad &= ~(KEY_R | KEY_L);
	new_buttons = pad ^ buttons; // set if button changed
	buttons = pad;

	if (new_buttons & KEY_B)
{
	if (!(buttons & KEY_B))
	{
		// X just released
		keyboard_handler(sc_Control, 0); // FIRE not pressed
		keyboard_handler(sc_Y, 0); // Y not pressed
	}
	else
	{
		// X just pressed
		keyboard_handler(sc_Control, 1); // FIRE pressed
		keyboard_handler(sc_Y, 1); // Y pressed
}
}
	if (new_buttons & KEY_X)
	{
		if (!(buttons & KEY_X))
		{
			// <| just released
			keyboard_handler(sc_Space, 0); // open/operate not pressed
		}
		else
		{
			// <| just pressed
			keyboard_handler(sc_Space, 1); // open/operate pressed
		}
	}
	if (new_buttons & KEY_Y)
	{
		if (!(buttons & KEY_Y))
		{
			// [] just released
			keyboard_handler(sc_RShift, 0); // run not pressed
		}
		else
		{
			// [] just pressed
			keyboard_handler(sc_RShift, 1); // run pressed
		}
	}

	if (new_buttons & KEY_DUP)
	{
		if (!(buttons & KEY_DUP))
		{
			// up just released
			keyboard_handler(sc_1, 0); // weapon 1 not pressed
			keyboard_handler(sc_UpArrow, 0); // up not pressed
		}
		else
		{
			// up just pressed
			if (buttons & KEY_A)
				keyboard_handler(sc_1, 1); // weapon 1 pressed
			else
				keyboard_handler(sc_UpArrow, 1); // up pressed
		}
	}
	if (new_buttons & KEY_DRIGHT)
	{
		if (!(buttons & KEY_DRIGHT))
		{
			// right just released
			keyboard_handler(sc_2, 0); // weapon 2 not pressed
			keyboard_handler(sc_RightArrow, 0); // right not pressed
		}
		else
		{
			// right just pressed
			if (buttons & KEY_A)
				keyboard_handler(sc_2, 1); // weapon 2 pressed
			else
				keyboard_handler(sc_RightArrow, 1); // right pressed
		}
	}
	if (new_buttons & KEY_DDOWN)
	{
		if (!(buttons & KEY_DDOWN))
		{
			// down just released
			keyboard_handler(sc_3, 0); // weapon 3 not pressed
			keyboard_handler(sc_DownArrow, 0); // down not pressed
		}
		else
		{
			// down just pressed
			if (buttons & KEY_A)
				keyboard_handler(sc_3, 1); // weapon 3 pressed
			else
				keyboard_handler(sc_DownArrow, 1); // down pressed
		}
	}
	if (new_buttons & KEY_DLEFT)
	{
		if (!(buttons & KEY_DLEFT))
		{
			// left just released
			keyboard_handler(sc_4, 0); // weapon 4 not pressed
			keyboard_handler(sc_LeftArrow, 0); // left not pressed
		}
		else
		{
			// left just pressed
			if (buttons & KEY_A)
				keyboard_handler(sc_4, 1); // weapon 4 pressed
			else
				keyboard_handler(sc_LeftArrow, 1); // left pressed
		}
	}
	if (new_buttons & KEY_L)
	{
		if (!(buttons & KEY_R))
		{
			// rtrg just released
			keyboard_handler(sc_Alt, 0); // alt not pressed
			keyboard_handler(sc_RightArrow, 0); // right not pressed
		}
		else
		{
			// rtrg just pressed
			keyboard_handler(sc_Alt, 1); // alt pressed
			keyboard_handler(sc_RightArrow, 1); // right pressed
		}
	}
	if (new_buttons & KEY_L)
	{
		if (!(buttons & KEY_L))
		{
			// ltrg just released
			keyboard_handler(sc_Alt, 0); // alt not pressed
			keyboard_handler(sc_LeftArrow, 0); // left not pressed
		}
		else
		{
			// ltrg just pressed
			keyboard_handler(sc_Alt, 1); // alt pressed
			keyboard_handler(sc_LeftArrow, 1); // left pressed
		}
	}
	if (new_buttons & KEY_START)
	{
		if (!(buttons & KEY_START))
		{
			// START just released
			keyboard_handler(sc_Escape, 0); // MENU not pressed
		}
		else
		{
			// START just pressed
			if (buttons & KEY_A)
			else
			keyboard_handler(sc_Escape, 1); // MENU pressed
		}
	}
	if (new_buttons & KEY_SELECT)
	{
		if (!(buttons & KEY_SELECT))
		{
			// SELECT just released
			keyboard_handler(sc_BackSpace, 0); // BackSpace not pressed
			keyboard_handler(sc_Enter, 0); // Enter not pressed
			keyboard_handler(sc_A, 0); // A not pressed
		}
		else
		{
			// SELECT just pressed
			if (buttons & KEY_A)
				keyboard_handler(sc_BackSpace, 1); // BackSpace pressed
			else if (buttons & KEY_Y)
				keyboard_handler(sc_A, 1); // A pressed
			else
				keyboard_handler(sc_Enter, 1); // Enter pressed
		}
	}
	if (new_buttons & KEY_A && !StartGame)
	{
		if (!(buttons & KEY_A))
		{
			// O just released
			keyboard_handler(sc_Escape, 0); // MENU not pressed
		}
		else
		{
			// O just pressed
			keyboard_handler(sc_Escape, 1); // MENU pressed
		}
	}

	// handle analog stick
	if (buttons & KEY_A)
	{
		/*if (pad.Ly < 64 && !mu)
		{
			// just pressed stick up
			mu = 1;
			keyboard_handler(sc_Tab, 1);
			keyboard_handler(sc_G, 1);
			keyboard_handler(sc_F10, 1); // toggle god-mode
		}
		else if (pad.Ly > 64 && mu)
		{
			// just released stick up
			mu = 0;
			keyboard_handler(sc_Tab, 0);
			keyboard_handler(sc_G, 0);
			keyboard_handler(sc_F10, 0); // toggle god-mode
		}
		if (pad.Ly > 192 && !md)
		{
			// just pressed stick down
			md = 1;
			keyboard_handler(sc_M, 1);
			keyboard_handler(sc_I, 1);
			keyboard_handler(sc_L, 1); // Full Health, Ammo, Keys, Weapons, Zero score
		}
		else if (pad.Ly < 192 && md)
		{
			// just released stick down
			md = 0;
			keyboard_handler(sc_M, 0);
			keyboard_handler(sc_I, 0);
			keyboard_handler(sc_L, 0);
		}*/
	}
	else
	{
		/*if (abs(pad.Lx-128) > 32)
			mx = (pad.Lx-128) >> 1;
		else
			mx = 0;
		if (abs(pad.Ly-128) > 32)
			my = (pad.Ly-128) >> 2;
		else
			my = 0;*/
	}
}

void IN_GetMouseDelta(int *dx, int *dy)
{
	if (dx)
		*dx = mx;
	if (dy)
		*dy = my;
}

byte IN_MouseButtons()
{
	return 0;
}
