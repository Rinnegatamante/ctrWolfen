#include "include/ff.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <3ds.h>
#ifdef USE_DMALLOC
#include<dmalloc.h>
#endif

static signed char **dirfname;
static int dirpos;
static int dirsize;

static int wildcard(const signed char *name, const signed char *match)
{
	int i;
	int max;
	max=strlen(name);
	if(strlen(match)!=max) {
		return 0;
	}
	for(i=0;i<max;i++) {
		signed char a,b;
		a=name[i];
		b=match[i];
		if(a>='a' && a<='z') a^=32;
		if(b>='a' && b<='z') b^=32;
		if(b=='?' || a==b) {
			// continue
		} else {
			return 0;
		}
	}
	return 1;
}

void unicodeToChar(signed char* dst, u16* src){
	if(!src || !dst)return;
	while(*src)*(dst++)=(*(src++))&0xFF;
	*dst=0x00;
}

typedef struct{
	u16 name[0x106];		 ///< UTF-16 encoded name
	u8	shortName[0x0A]; ///< 8.3 File name
	u8	shortExt[0x04];	///< 8.3 File extension (set to spaces for directories)
	u8	unknown2;				///< ???
	u8	unknown3;				///< ???
	u8	isDirectory;		 ///< Directory bit
	u8	isHidden;				///< Hidden bit
	u8	isArchive;			 ///< Archive bit
	u8	isReadOnly;			///< Read-only bit
	u64 fileSize;				///< File size
} FS_dirent;

int findfirst(const signed char *pathname, struct ffblk *ffblk, int attrib) {
	signed char *match=strdup(pathname);
	unsigned int i;
	if(match[0]=='*') match++;
	for(i=0;i<strlen(match);i++) { 
		if(match[i]>='a' && match[i]<='z') match[i]^=32; 
	}
	dirsize=0;
	printf("Looking for '%s' (%s)\n",match,pathname);
	Handle dirHandle;
	FS_dirent entry;
	FS_Path dirPath=fsMakePath(PATH_ASCII, "/3ds/ctrWolfen");
	FS_Archive sdmcArchive = (FS_Archive){0x9, (FS_Path){PATH_EMPTY, 1, (u8*)""}};
	FSUSER_OpenArchive( &sdmcArchive);
	FSUSER_OpenDirectory(&dirHandle, sdmcArchive, dirPath);
	u32 entriesRead;
	for (;;){
		entriesRead=0;
		FSDIR_Read(dirHandle, &entriesRead, 1, &entry);
		static signed char name[64];
		if (entriesRead){
			unicodeToChar(name,entry.name);
			if(strcasestr(name,match)==0 && wildcard(name,match)==0) continue;
			if(dirsize==0) {
				dirfname=(signed char **)calloc(sizeof(signed char *),64);
			} else if((dirsize%64)==0) {
				dirfname=(signed char **)realloc(dirfname,sizeof(signed char *)*(dirsize+64));
			}
			dirfname[dirsize++]=strdup(name);
		}else break;
	}
	FSDIR_Close(dirHandle);
	FSUSER_CloseArchive( &sdmcArchive);
	printf("Got %d matches\n",dirsize);
	if (dirsize>0) {
		dirpos=1;
		strcpy(ffblk->ff_name,dirfname[dirsize-1]);
		return 0;
	}

	return 1;

}

int findnext(struct ffblk *ffblk) {

  if (dirpos<dirsize) {
    strcpy(ffblk->ff_name,dirfname[dirpos++]);
    return 0;
  }
  return 1;

}

void resetinactivity(void) {
	//User::ResetInactivityTime();
}
