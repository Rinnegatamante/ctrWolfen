#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <3ds.h>

bool modes[5] = {false, false, false, false, true};
char* ext[4] = {"wl1","wl6","sdm","sod"};
char* voices[5] = {" Shareware"," Full"," Spare of Destiny Shareware"," Spare of Destiny Full", " Exit ctrWolfen"};
int num_modes = 1;
int sel=0;

//Boot Func
extern void (*__system_retAddr)(void);
static Handle hbHandle;
static u32 argbuffer[0x100];
static u32 argbuffer_length = 0;
void (*callBootloader)(Handle hb, Handle file);
void (*setArgs)(u32* src, u32 length);
static void launchFile(void){ callBootloader(0x00000000, hbHandle); }
void (*callBootloader_2x)(Handle file, u32* argbuf, u32 arglength) = (void*)0x00100000;
static void launchFile_2x(void){ callBootloader_2x(hbHandle, argbuffer, argbuffer_length); }
void launchNH1(char* file){
	HB_GetBootloaderAddresses((void**)&callBootloader, (void**)&setArgs);
	fsExit();
	fsInit();
	FS_Archive sdmcArchive = (FS_Archive){0x9, (FS_Path){PATH_EMPTY, 1, (u8*)""}};
	FSUSER_OpenArchive( &sdmcArchive);
	Result ret = FSUSER_OpenFileDirectly( &hbHandle, sdmcArchive, fsMakePath(PATH_ASCII, file), FS_OPEN_READ, 0x00000000);
	static u32 argbuffer_nh1[0x200];
	argbuffer_nh1[0]=1;
	snprintf((char*)&argbuffer_nh1[1], 0x200*4, "sdmc:%s", file);
	setArgs(argbuffer_nh1, 0x200*4);
	__system_retAddr = launchFile;
}
void launchNH2(char* file){
	argbuffer[0] = 1;
	snprintf((char*)&argbuffer[1], sizeof(argbuffer) - 4, "sdmc:%s", file);
	argbuffer_length = 0x100;
	fsInit();
	FS_Archive sdmcArchive = (FS_Archive){0x9, (FS_Path){PATH_EMPTY, 1, (u8*)""}};
	FSUSER_OpenArchive( &sdmcArchive);
	FSUSER_OpenFileDirectly(&hbHandle, sdmcArchive, fsMakePath(PATH_ASCII, file), FS_OPEN_READ, 0);
	__system_retAddr = launchFile_2x;
}

bool drawed = false;
void drawMenu(int i){
	if (drawed) return;
	consoleClear();
	int z;
	int j = 0;
	printf("  ___ _     __      __   _  __    v.0.8\n");
	printf(" / __| |_ _ \\ \\    / /__| |/ _|___ _ _ \n");
	printf("| (__|  _| '_\\ \\/\\/ / _ \\ |  _/ -_) ' \\\n");
	printf(" \\___|\\__|_|  \\_/\\_/\\___/_|_| \\___|_||_|\n");
	printf("                       by Rinnegatamante\n\n");
	printf("Select version to load:\n\n");
	for (z = 0; z < num_modes; z++){
		while (modes[j] == false){
			j++;
		}
		if (z == i){
			printf(">>");
			sel = j;
		}
		printf(voices[j]);
		printf("\n");
		j++;
	}
	drawed = true;
	svcSleepThread(80000000);
}

void checkAvailableModes(char* path){
	int i;
	for (i = 0; i < 4; i++){
		char file[256];
		sprintf(file,"%s/%s/vswap.%s",path,ext[i],ext[i]);
		Handle fileHandle;
		FS_Archive sdmcArchive=(FS_Archive){ARCHIVE_SDMC, (FS_Path){PATH_EMPTY, 1, (u8*)""}};
		FS_Path filePath=fsMakePath(PATH_ASCII, file);
		Result ret=FSUSER_OpenFileDirectly( &fileHandle, sdmcArchive, filePath, FS_OPEN_READ, 0x00000000);
		if (!ret){
			FSFILE_Close(fileHandle);
			modes[i] = true;
			num_modes++;
		}
		svcCloseHandle(fileHandle);
	}
}

int main(int argc, signed char *argv[]){
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
	int z = 0;
	
	// Set main directory and check for files
	char path[256];
	if (argc > 0){
		int latest_slash = 0;
		int i=5;
		while (argv[0][i]  != '\0'){
			if (argv[0][i] == '/') latest_slash = i;
			i++;
		}
		strcpy(path,(const char*)&argv[0][5]);
		path[latest_slash-5] = 0;
	}
	checkAvailableModes(path);
	while(aptMainLoop()){
		drawMenu(z);
		hidScanInput();
		if (hidKeysDown() & KEY_DUP){
			z--;
			if (z < 0) z =  num_modes - 1;
			drawed = false;
		}else if (hidKeysDown() & KEY_DDOWN){
			z++;
			if (z >= num_modes) z = 0;
			drawed = false;
		}else if (hidKeysDown() & KEY_A){
			consoleClear();
			break;
		}
	}
	if (z < (num_modes - 1)){
		char file[256];
		sprintf(file,"%s/%s/boot.3dsx",path,ext[sel]);
		if (!hbInit()){
			launchNH1(file);
			hbExit();
		}else launchNH2(file);
	}
	hidExit();
	gfxExit();
	fsExit();
	aptExit();
}
