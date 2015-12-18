/* input/video output "/dev/null" support */

/* this file does nothing but sit there and look pretty dumb */

#include "include/wl_def.h"

byte *gfxbuf = NULL;

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
		/* printf("spiffy ansi screen goes here..\n"); */
	}
	
	if (error && *error) {
		fprintf(stderr, "Quit: %s\n", error);
		exit(EXIT_FAILURE);
 	}
	exit(EXIT_SUCCESS);
}

void VL_WaitVBL(int vbls)
{
}

void VW_UpdateScreen()
{
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
	if (gfxbuf == NULL) 
		gfxbuf = malloc(320 * 200 * 1);		
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
	if (gfxbuf != NULL) {
		free(gfxbuf);
		gfxbuf = NULL;
	}
}

//===========================================================================

/*
=================
=
= VL_SetPalette
=
=================
*/

void VL_SetPalette(const byte *palette)
{
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
}

void INL_Update()
{
}

int main(int argc, char *argv[])
{
	vwidth = 320;
	vheight = 200;
	
	return WolfMain(argc, argv);
}
