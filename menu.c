#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>

#include <unistd.h> /* test! for chdir etc. */


#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <time.h>

#include "gnuboy/Version"
#include "ohboy_ver.h"

#include "gnuboy.h"

#include "fb.h"
#include "ubytegui/gui.h"
#include "ubytegui/dialog.h"
#include "input.h"
#include "hw.h"
#include "rc.h"
#include "loader.h"
#include "mem.h"
#include "sound.h"
#include "lcd.h"
#include "menu.h"


extern void scaler_init(int scaler_number); /* maybe keeping this and loosing the other stuff */

#ifdef DINGOO_NATIVE

#include <jz4740/cpu.h>  /* for cpu clock */

char *ctime(const time_t *timep);
char *ctime(const time_t *timep)
{
    char *x="not_time";
    return x;
}

#endif /* DINGOO_NATIVE */


char *menu_getext(char *s){
	char* ext = NULL;
	while(*s){
		if(*s++ == '.') ext=s;
	}

	return ext;
}

int filterfile(char *p, char *exts){
	char* ext;
	char* cmp;
	int n,i;

	if(exts==NULL) return 1;

	ext = exts;
	cmp = menu_getext(p);

	if(cmp==NULL) return 0;

	while(*ext != 0){
		n=0;
		while(*exts++ != ';'){
			n++;
			if(*exts==0) break;
		}

		i=0;
		while(tolower(cmp[i])==tolower(ext[i])){
			i++;
			if((i==n) && (cmp[i]==0)) return 1;
			if((i==n) || (cmp[i]==0)) break;
		}
		ext = exts;
	}
	return 0;
}

int fcompare(const void *a, const void *b){
	char *stra = *(char**)a;
	char *strb = *(char**)b;
	int cmp;
	return strcmp(stra,strb);
}

char* menu_browsedir(char* fpathname, char* file, char *title, char *exts){
    /* this routine has side effects with fpathname and file, FIXME */
	DIR *dir;
	struct dirent *d;
	int n=0, i, j;
	char *files[1<<16];  /* 256Kb */
#ifndef  DT_DIR
	struct stat s;
	char tmpfname[PATH_MAX];
	char *tmpfname_end;
#endif /* DT_DIR */


	if(!(dir = opendir(fpathname))) return NULL; /* FIXME try parent(s) until out of paths */

	/* TODO FIXME add root directory check (to avoid adding .. as a menu option when at "/" or "x:\") */
	files[n] = malloc(4);
	strcpy(files[n], "..");
	strcat(files[n], DIRSEP);
	n++;

	d = readdir(dir);
	if(d && !strcmp(d->d_name,".")) d = readdir(dir);
	if(d && !strcmp(d->d_name,"..")) d = readdir(dir);

#ifndef  DT_DIR
	strcpy(tmpfname, fpathname);
	tmpfname_end = &tmpfname[0];
	tmpfname_end += strlen(tmpfname);
#endif /* DT_DIR */

	while(d){
#ifndef  DT_DIR
		/* can not lookup type from search result have to stat filename*/
		strcpy(tmpfname_end, d->d_name);
		stat(tmpfname, &s);
		if(S_ISDIR (s.st_mode))
#else
		if ((d->d_type & DT_DIR) == DT_DIR)
#endif /* DT_DIR */
		{
			files[n] = malloc(strlen(d->d_name)+2);
			strcpy(files[n], d->d_name);
			strcat(files[n], DIRSEP);
			n++;
		} else if(filterfile(d->d_name,exts)){
			files[n] = malloc(strlen(d->d_name)+1);
			strcpy(files[n], d->d_name);
			n++;
		}
		d  = readdir(dir);
	}
	closedir (dir);
	qsort(files+1,n-1,sizeof(char*),fcompare);

	dialog_begin(title, fpathname);

	for(i=0; i<n; i++){
		dialog_text(files[i],"",FIELD_SELECTABLE);
	}

	if(j = dialog_end()){
		if(file) {
			strcpy(file,files[j-1]);
		} else {
			file = files[j-1];
			files[j-1] = NULL;
		}
	}

	for(i=0; i<n; i++){
		free(files[i]);
	}

	return j ? file : NULL;

}

char* menu_requestfile(char* file, char *title, char* path, char *exts){
	char *dir;
	int allocmem = file == NULL;
	int tmplen = 0;
	char parent_dir_str[5];
#ifdef DINGOO_NATIVE
    static char dingoo_default_path[]="A:\\";
#endif /* DINGOO_NATIVE */

    /* TODO clear all the dyanamic memory allocations in this routine and require caller to pre-allocate */

	if (allocmem) file = malloc(PATH_MAX);
#ifdef DEBUG_ALWAYS_RETURN_ADJUSTRIS_GB
    strcpy(file, "adjustris.gb");
    return file;
#endif /* DEBUG_ALWAYS_RETURN_ADJUSTRIS_GB */
	if(path)
	{
		strcpy(file, path);
		tmplen = strlen(file);
		if (tmplen >=1)
		{
			if (file[tmplen-1] != DIRSEP_CHAR)
				strcat(file, DIRSEP); /* this is very fragile, e.g. what if dir was specified but does not exist */
		}
	}
	else
		strcpy(file, "");
    
	snprintf(parent_dir_str, sizeof(parent_dir_str), "..%s", DIRSEP);

	while(dir = menu_browsedir(file, file+strlen(file),title,exts)){
        if (!strcmp(dir, parent_dir_str))
        {
            /* need to go up a directory */
            dir--;
            if (dir > file)
            {
                *dir = '\0';
                while (dir >= file && *dir != DIRSEP_CHAR)
                {
                    *dir = '\0';
                    dir--;
                }
            }
            if (strlen(file) == 0)
            {
#ifdef DINGOO_NATIVE
                if (dingoo_default_path[0] == 'A')
                    dingoo_default_path[0] = 'B';
                else
                    dingoo_default_path[0] = 'A';
                sprintf(file, "%s", dingoo_default_path);
                /*
                **  If there is no MiniSD card in B:
                **  then an empty list is displayed
                **  click the parent directory to
                **  toggle back to A:
                */
#else
                sprintf(file, ".%s", DIRSEP);
#endif /* DINGOO_NATIVE */
                dir = file + strlen(file)+1;
                *dir = '\0';
            }
        }
		/*
		** Check to see if we have a file name,
		** or a new directory name to scan
		** Directory name will have trailing path sep character
		** FIXME check for "../" and dirname the dir
		*/
		tmplen = strlen(dir);
		if (tmplen >=1)
			if (dir[tmplen-1] != DIRSEP_CHAR)
			{
				if (allocmem) file = realloc(file, strlen(file)+1);
				break;
			}
	}
	if(!dir) file = NULL; /* FIXME what it file was not null when function was called, what about free'ing this is we allocated it? */

	return file;
}

char *menu_requestdir(const char *title, const char *path){
	char *dir=NULL, **ldirs;
	int ldirsz, ldirn=0, ret, l;
	DIR *cd;
	struct dirent *d;
#ifndef  DT_DIR
	struct stat s;
#endif /* DT_DIR */
	char *cdpath;
    
//#ifndef  DT_DIR
	char tmpfname[PATH_MAX];
	char *tmpfname_end;
//#endif /* DT_DIR */

	cdpath = malloc(PATH_MAX);

	strcpy(cdpath, path);
//#ifndef  DT_DIR
	strcpy(tmpfname, path);
	tmpfname_end = &tmpfname[0];
	tmpfname_end += strlen(tmpfname);
//#endif /* DT_DIR */

	while(!dir){
		cd = opendir(cdpath);

		dialog_begin(title, cdpath);
		dialog_text("[Select This Directory]",NULL,FIELD_SELECTABLE);
		dialog_text("/.. Parent Directory",NULL,FIELD_SELECTABLE);

		ldirsz = 16;
		ldirs = malloc(sizeof(char*)*ldirsz);

		d = readdir(cd);
		if(d && !strcmp(d->d_name,".")) d = readdir(cd);
		if(d && !strcmp(d->d_name,"..")) d = readdir(cd);

		while(d){
			if(ldirn >= ldirsz){
				ldirsz += 16;
				ldirs = realloc(ldirs,ldirsz*sizeof(char*));
			}

#ifndef  DT_DIR
			/* can not lookup type from search result have to stat filename*/
			strcpy(tmpfname_end, d->d_name);
			stat(tmpfname, &s);
			if(S_ISDIR (s.st_mode))
#else
			if ((d->d_type & DT_DIR) == DT_DIR)
#endif /* DT_DIR */
			{
				l = strlen(d->d_name);
				ldirs[ldirn] = malloc(l+2);
				strcpy(ldirs[ldirn], d->d_name);
				ldirs[ldirn][l] = DIRSEP_CHAR;
				ldirs[ldirn][l+1] = '\0';

				dialog_text(ldirs[ldirn],NULL,FIELD_SELECTABLE);
				ldirn++;
			}

			d = readdir(cd);
		}
		closedir(cd);

		switch(ret=dialog_end()){
			case 0:
				dir = (char*)-1;
				break;
			case 1:
				dir = strdup(cdpath);
				break;
			case 2:
				{
					/* need to go up a directory */
					char *tmp_str=NULL;
					tmp_str=cdpath;
					tmp_str+=strlen(tmp_str)-1;
					tmp_str--;
					if (tmp_str > cdpath)
					{
						*tmp_str = '\0';
						while (tmp_str >= cdpath && *tmp_str != DIRSEP_CHAR)
						{
							*tmp_str = '\0';
							tmp_str--;
						}
					}
					if (strlen(cdpath) == 0)
					{
						sprintf(cdpath, ".%s", DIRSEP);
						tmp_str = cdpath + strlen(cdpath)+1;
						*tmp_str = '\0';
					}
					strcpy(tmpfname, cdpath);
					tmpfname_end = &tmpfname[0];
					tmpfname_end += strlen(tmpfname);
				}
				break;
			default:
				strcpy(tmpfname_end, ldirs[ret-3]);
				tmpfname_end = &tmpfname[0];
				tmpfname_end += strlen(tmpfname);
				strcpy(cdpath, tmpfname);
				break;
		}

		while(ldirn)
			free(ldirs[--ldirn]);
		free(ldirs);
	}

	if(dir==(char*)-1)
		dir = NULL;

	free(cdpath);

	return dir;
}

/* NOTE number of slot entries is indirectly related to font size */
/* only display one screen of save slots */
static const char *slots[] = {
    "Slot 0","Slot 1","Slot 2","Slot 3","Slot 4","Slot 5","Slot 6","Slot 7",
    "Slot 8","Slot 9","Slot 10","Slot 11","Slot 12","Slot 13","Slot 14","Slot 15",
    NULL};
static const char *emptyslot = "<Empty>";
static const char *not_emptyslot = "<Used>";

int menu_state(int save){

	char **statebody=NULL;
	char* name;

	int i, flags,ret, del=0,l;
#ifndef OHBOY_FILE_STAT_NOT_AVAILABLE
	/* Not all platforms implement stat()/fstat() */
	struct stat fstat;
	time_t time;
	char *tstr;
#endif

	char *savedir;
	char *savename;
	char *saveprefix;
	FILE *f;
	int sizeof_slots=0;
    while (slots[sizeof_slots] != NULL)
        sizeof_slots++;
    statebody = malloc(sizeof_slots * sizeof(char*));  /* FIXME check for NULL return from malloc */

	savedir = rc_getstr("savedir");
	savename = rc_getstr("savename");
	saveprefix = malloc(strlen(savedir) + strlen(savename) + 2);
	sprintf(saveprefix, "%s%s%s", savedir, DIRSEP, savename);

	dialog_begin(save?"Save State":"Load State",rom.name);

	for(i=0; i<sizeof_slots; i++){

		name = malloc(strlen(saveprefix) + 5);
		sprintf(name, "%s.%03d", saveprefix, i);

#ifndef OHBOY_FILE_STAT_NOT_AVAILABLE
		/* if the file exists lookup the timestamp */
		if(!stat(name,&fstat)){
			time = fstat.st_mtime;
			tstr = ctime(&time);

			l = strlen(tstr);
			statebody[i] = malloc(l);
			strcpy(statebody[i],tstr);
			statebody[i][l-1]=0;
#else
		/* check if the file exists */
		if(f=fopen(name,"rb")){
			fclose(f);
			statebody[i] = (char*)not_emptyslot;
#endif /* OHBOY_FILE_STAT_NOT_AVAILABLE */
			flags = FIELD_SELECTABLE;
		} else {
			statebody[i] = (char*)emptyslot;
			flags = save ? FIELD_SELECTABLE : 0;
		}
		dialog_text(slots[i],statebody[i],flags);

		free(name);
	}

	if(ret=dialog_end()){
		name = malloc(strlen(saveprefix) + 5);
		sprintf(name, "%s.%03d", saveprefix, ret-1);
		if(save){
			if(f=fopen(name,"wb")){
				savestate(f);
				fclose(f);
			}
		}else{
			if(f=fopen(name,"rb")){
				loadstate(f);
				fclose(f);
				vram_dirty();
				pal_dirty();
				sound_dirty();
				mem_updatemap();
			}
		}
		free(name);
	}

	for(i=0; i<sizeof_slots; i++)
		if(statebody[i] != emptyslot && statebody[i] != not_emptyslot) free(statebody[i]);

	free(saveprefix);
	return ret;
}

#define GBPAL_COUNT 27
struct pal_s{
	char name[16];
	unsigned int dmg_bgp[4];
	unsigned int dmg_wndp[4];
	unsigned int dmg_obp0[4];
	unsigned int dmg_obp1[4];
}gbpal[GBPAL_COUNT] = {
	{
		.name = "Default",
		.dmg_bgp  = {0X98D0E0,0X68A0B0,0X60707C,0X2C3C3C},
		.dmg_wndp = {0X98D0E0,0X68A0B0,0X60707C,0X2C3C3C},
		.dmg_obp0 = {0X98D0E0,0X68A0B0,0X60707C,0X2C3C3C},
		.dmg_obp1 = {0X98D0E0,0X68A0B0,0X60707C,0X2C3C3C}
	},{//Grey Palette
		.name = "Grey",
		.dmg_bgp  = {  0xFFFFFF, 0xAAAAAA, 0x555555, 0x000000 }, //BG
		.dmg_wndp = {  0xFFFFFF, 0xAAAAAA, 0x555555, 0x000000 }, //WIN
		.dmg_obp0 = {  0xFFFFFF, 0xAAAAAA, 0x555555, 0x000000 }, //OB0
		.dmg_obp1 = {  0xFFFFFF, 0xAAAAAA, 0x555555, 0x000000 }  //OB1
	},{//Realistic Palette
		.name = "DMG",
		.dmg_bgp  = {   0x006B5C, 0x265808, 0x343E08, 0x222004 },
		.dmg_wndp = {   0x006B5C, 0x265808, 0x343E08, 0x222004 },
		.dmg_obp0 = {   0x006B5C, 0x265808, 0x343E08, 0x222004 },
		.dmg_obp1 = {   0x006B5C, 0x265808, 0x343E08, 0x222004 }
	},{//BGB Emulator Palette
		.name = "BGB",
		.dmg_bgp  = {   0xD0F8E0, 0x70C088, 0x506830, 0x201808 },
		.dmg_wndp = {   0xD0F8E0, 0x70C088, 0x506830, 0x201808 },
		.dmg_obp0 = {   0xD0F8E0, 0x70C088, 0x506830, 0x201808 },
		.dmg_obp1 = {   0xD0F8E0, 0x70C088, 0x506830, 0x201808 }
	},{//KiGB Emulator Palette
		.name = "KiGB",
		.dmg_bgp  = {   0x50D050, 0x40A040, 0x307030, 0x204020 },
		.dmg_wndp = {   0x50D050, 0x40A040, 0x307030, 0x204020 },
		.dmg_obp0 = {   0x50D050, 0x40A040, 0x307030, 0x204020 },
		.dmg_obp1 = {   0x50D050, 0x40A040, 0x307030, 0x204020 }
	},{//NO$GMB Emulator Palette
		.name = "NO$GMB",
		.dmg_bgp  = {   0x88E0F8, 0x58B0D8, 0x387898, 0x183848 },
		.dmg_wndp = {   0x88E0F8, 0x58B0D8, 0x387898, 0x183848 },
		.dmg_obp0 = {   0x88E0F8, 0x58B0D8, 0x387898, 0x183848 },
		.dmg_obp1 = {   0x88E0F8, 0x58B0D8, 0x387898, 0x183848 }
	},{//VGB Emulator Palette
		.name = "VGB",
		.dmg_bgp  = {   0xADF7FF, 0x73AEB5, 0x315963, 0x000810 },
		.dmg_wndp = {   0xADF7FF, 0x73AEB5, 0x315963, 0x000810 },
		.dmg_obp0 = {   0xADF7FF, 0x73AEB5, 0x315963, 0x000810 },
		.dmg_obp1 = {   0xADF7FF, 0x73AEB5, 0x315963, 0x000810 }
	},{//Gameboy Pocket Palette B
		.name = "GBPocket A",
		.dmg_bgp  = {   0x96B496, 0x6F8265, 0x485134, 0x222004 },
		.dmg_wndp = {   0x96B496, 0x6F8265, 0x485134, 0x222004 },
		.dmg_obp0 = {   0x96B496, 0x6F8265, 0x485134, 0x222004 },
		.dmg_obp1 = {   0x96B496, 0x6F8265, 0x485134, 0x222004 }
	},{//Gameboy Pocket Palette B
		.name = "GBPocket B",
		.dmg_bgp  = {   0x78A591, 0x5B7862, 0x3E4C33, 0x222004 },
		.dmg_wndp = {   0x78A591, 0x5B7862, 0x3E4C33, 0x222004 },
		.dmg_obp0 = {   0x78A591, 0x5B7862, 0x3E4C33, 0x222004 },
		.dmg_obp1 = {   0x78A591, 0x5B7862, 0x3E4C33, 0x222004 }
	},{//Hot Palette
		.name = "Hot",
		.dmg_bgp  = {   0x6496D2, 0x4E6E8D, 0x384748, 0x222004 },
		.dmg_wndp = {   0x6496D2, 0x4E6E8D, 0x384748, 0x222004 },
		.dmg_obp0 = {   0x6496D2, 0x4E6E8D, 0x384748, 0x222004 },
		.dmg_obp1 = {   0x6496D2, 0x4E6E8D, 0x384748, 0x222004 }
	},{//Warm Palette
		.name = "Warm",
		.dmg_bgp  = {   0x64D2BE, 0x4E9680, 0x385B42, 0x222004 },
		.dmg_wndp = {   0x64D2BE, 0x4E9680, 0x385B42, 0x222004 },
		.dmg_obp0 = {   0x64D2BE, 0x4E9680, 0x385B42, 0x222004 },
		.dmg_obp1 = {   0x64D2BE, 0x4E9680, 0x385B42, 0x222004 }
	},{//Fresh Palette
		.name = "Fresh",
		.dmg_bgp  = {   0xBED264, 0x8A9644, 0x565B24, 0x222004 },
		.dmg_wndp = {   0xBED264, 0x8A9644, 0x565B24, 0x222004 },
		.dmg_obp0 = {   0xBED264, 0x8A9644, 0x565B24, 0x222004 },
		.dmg_obp1 = {   0xBED264, 0x8A9644, 0x565B24, 0x222004 }
	},{//Cold Palette
		.name = "Cold",
		.dmg_bgp  = {   0xD29664, 0x976E44, 0x5C4724, 0x222004 },
		.dmg_wndp = {   0xD29664, 0x976E44, 0x5C4724, 0x222004 },
		.dmg_obp0 = {   0xD29664, 0x976E44, 0x5C4724, 0x222004 },
		.dmg_obp1 = {   0xD29664, 0x976E44, 0x5C4724, 0x222004 }
	},{//Tinted Palette A
		.name = "Tinted A",
		.dmg_bgp  = {   0xA1CFC4, 0x6D958B, 0x3C534D, 0x1F1F1F },
		.dmg_wndp = {   0xA1CFC4, 0x6D958B, 0x3C534D, 0x1F1F1F },
		.dmg_obp0 = {   0xA1CFC4, 0x6D958B, 0x3C534D, 0x1F1F1F },
		.dmg_obp1 = {   0xA1CFC4, 0x6D958B, 0x3C534D, 0x1F1F1F }
	},{//Tinted Palette B
		.name = "Tinted B",
		.dmg_bgp  = {   0xDCFFE6, 0x96B9A0, 0x55735A, 0x0F280F },
		.dmg_wndp = {   0xDCFFE6, 0x96B9A0, 0x55735A, 0x0F280F },
		.dmg_obp0 = {   0xDCFFE6, 0x96B9A0, 0x55735A, 0x0F280F },
		.dmg_obp1 = {   0xDCFFE6, 0x96B9A0, 0x55735A, 0x0F280F }
	},{//Tinted Palette C
		.name = "Tinted C",
		.dmg_bgp  = {   0x009284, 0x00786E, 0x00554B, 0x002D28 },
		.dmg_wndp = {   0x009284, 0x00786E, 0x00554B, 0x002D28 },
		.dmg_obp0 = {   0x009284, 0x00786E, 0x00554B, 0x002D28 },
		.dmg_obp1 = {   0x009284, 0x00786E, 0x00554B, 0x002D28 }
	},{//Left
		.name = "Blue",
		.dmg_bgp  = {   0xFFFFFF, 0xF8A878, 0xF8A878, 0x000000 },
		.dmg_wndp = {   0xFFFFFF, 0xF83000, 0xF83000, 0x000000 },
		.dmg_obp0 = {   0xFFFFFF, 0x8888E8, 0x004080, 0x000000 },
		.dmg_obp1 = {   0xFFFFFF, 0x8888E8, 0x004080, 0x000000 }
	},{//Left+A
		.name = "Dark Blue",
		.dmg_bgp  = {   0xFFFFFF, 0xF6939D, 0xA03346, 0x000000 },
		.dmg_wndp = {   0xFFFFFF, 0xF6939D, 0xA03346, 0x000000 },
		.dmg_obp0 = {   0xFFFFFF, 0x8888E8, 0x004080, 0x000000 },
		.dmg_obp1 = {   0xFFFFFF, 0x8888E8, 0x004080, 0x000000 }
	},{//Up
		.name = "Brown",
		.dmg_bgp  = {   0xFFFFFF, 0x5098E8, 0x004080, 0x000000 },
		.dmg_wndp = {   0xFFFFFF, 0x5098E8, 0x004080, 0x000000 },
		.dmg_obp0 = {   0xFFFFFF, 0x5098E8, 0x004080, 0x000000 },
		.dmg_obp1 = {   0xFFFFFF, 0x5098E8, 0x004080, 0x000000 }
	},{//Up+A
		.name = "Red",
		.dmg_bgp  = {   0xFFFFFF, 0x8888E8, 0x2727A8, 0x000000 },
		.dmg_wndp = {   0xFFFFFF, 0x8888E8, 0x2727A8, 0x000000 },
		.dmg_obp0 = {   0xFFFFFF, 0x00F800, 0xFF3300, 0x000000 },
		.dmg_obp1 = {   0xFFFFFF, 0x00F800, 0xFF3300, 0x000000 }
	},{//Up+B
		.name = "Dark Brown",
		.dmg_bgp  = {   0xFFFFFF, 0x94ACC0, 0x4A7B94, 0x000000 },
		.dmg_wndp = {   0xFFFFFF, 0x94ACC0, 0x4A7B94, 0x000000 },
		.dmg_obp0 = {   0xFFFFFF, 0x5098E8, 0x004080, 0x000000 },
		.dmg_obp1 = {   0xFFFFFF, 0x5098E8, 0x004080, 0x000000 }
	},{//Right
		.name = "Green",
		.dmg_bgp  = {   0xFFFFFF, 0x00F800, 0x0033F8, 0x000000 },
		.dmg_wndp = {   0xFFFFFF, 0x00F800, 0x0033F8, 0x000000 },
		.dmg_obp0 = {   0xFFFFFF, 0x00F800, 0x0033F8, 0x000000 },
		.dmg_obp1 = {   0xFFFFFF, 0x00F800, 0x0033F8, 0x000000 }
	},{//Right+A
		.name = "Dark Green",
		.dmg_bgp  = {   0xFFFFFF, 0x00F800, 0x0033F8, 0x000000 },
		.dmg_wndp = {   0xFFFFFF, 0x00F800, 0x0033F8, 0x000000 },
		.dmg_obp0 = {   0xFFFFFF, 0xE88888, 0x2727A8, 0x000000 },
		.dmg_obp1 = {   0xFFFFFF, 0xE88888, 0x2727A8, 0x000000 }
	},{//Right+B
		.name = "Inverted",
		.dmg_bgp  = {   0x000000, 0xA1A200, 0x00FFF0, 0xFFFFFF },
		.dmg_wndp = {   0x000000, 0xA1A200, 0x00FFF0, 0xFFFFFF },
		.dmg_obp0 = {   0x000000, 0xA1A200, 0x00FFF0, 0xFFFFFF },
		.dmg_obp1 = {   0x000000, 0xA1A200, 0x00FFF0, 0xFFFFFF }
	},{//Down
		.name = "Pastel",
		.dmg_bgp  = {   0xFFFFFF, 0x8888E8, 0xF6939D, 0x000000 },
		.dmg_wndp = {   0xFFFFFF, 0x8888E8, 0xF6939D, 0x000000 },
		.dmg_obp0 = {   0xFFFFFF, 0x8888E8, 0xF6939D, 0x000000 },
		.dmg_obp1 = {   0xFFFFFF, 0x8888E8, 0xF6939D, 0x000000 }
	},{//Down+A
		.name = "Orange",
		.dmg_bgp  = {   0xFFFFFF, 0x00FFF0, 0x0033F8, 0x000000 },
		.dmg_wndp = {   0xFFFFFF, 0x00FFF0, 0x0033F8, 0x000000 },
		.dmg_obp0 = {   0xFFFFFF, 0x00FFF0, 0x0033F8, 0x000000 },
		.dmg_obp1 = {   0xFFFFFF, 0x00FFF0, 0x0033F8, 0x000000 }
	},{//Down+B
		.name = "Yellow",
		.dmg_bgp  = {   0xFFFFFF, 0x00FFF8, 0x004080, 0x000000 },
		.dmg_wndp = {   0xFFFFFF, 0x00FFF8, 0x004080, 0x000000 },
		.dmg_obp0 = {   0xFFFFFF, 0xF8A878, 0x008848, 0x000000 },
		.dmg_obp1 = {   0xFFFFFF, 0xF8A878, 0x008848, 0x000000 }
	}
};

int findpal(){
	int *a, *b;
	int i,j;
	for(i=0; i<GBPAL_COUNT; i++){
		a = gbpal[i].dmg_bgp ; b = rc_getvec("dmg_bgp");
		if(a[0] != b[0] || a[1] != b[1] || a[2] != b[2] || a[3] != b[3])
			continue;
		a = gbpal[i].dmg_wndp; b = rc_getvec("dmg_wndp");
		if(a[0] != b[0] || a[1] != b[1] || a[2] != b[2] || a[3] != b[3])
			continue;
		a = gbpal[i].dmg_obp0; b = rc_getvec("dmg_obp0");
		if(a[0] != b[0] || a[1] != b[1] || a[2] != b[2] || a[3] != b[3])
			continue;
		a = gbpal[i].dmg_obp1; b = rc_getvec("dmg_obp1");
		if(a[0] != b[0] || a[1] != b[1] || a[2] != b[2] || a[3] != b[3])
			continue;
		return i;
	}
	return 0;
}

char *lpalettes[] = {
	gbpal[0].name,
	gbpal[1].name,
	gbpal[2].name,
	gbpal[3].name,
	gbpal[4].name,
	gbpal[5].name,
	gbpal[6].name,
	gbpal[7].name,
	gbpal[8].name,
	gbpal[9].name,
	gbpal[10].name,
	gbpal[11].name,
	gbpal[12].name,
	gbpal[13].name,
	gbpal[14].name,
	gbpal[15].name,
	gbpal[16].name,
	gbpal[17].name,
	gbpal[18].name,
	gbpal[19].name,
	gbpal[20].name,
	gbpal[21].name,
	gbpal[22].name,
	gbpal[23].name,
	gbpal[24].name,
	gbpal[25].name,
	gbpal[26].name,
	NULL
};

const char *lbutton_a[] = {"None","Button A","Button B","Select","Start","Reset","Quit",NULL};
const char *lbutton_b[] = {"None","Button A","Button B","Select","Start","Reset","Quit",NULL};
const char *lbutton_x[] = {"None","Button A","Button B","Select","Start","Reset","Quit",NULL};
const char *lbutton_y[] = {"None","Button A","Button B","Select","Start","Reset","Quit",NULL};
const char *lbutton_l[] = {"None","Button A","Button B","Select","Start","Reset","Quit",NULL};
const char *lbutton_r[] = {"None","Button A","Button B","Select","Start","Reset","Quit",NULL};
const char *lcolorfilter[] = {"Off","On","GBC Only",NULL};
const char *lupscaler[] = {"Native (No scale)", "Sample1.5x", "Scale3x+Sample.75x", "Ayla Fullscreen", NULL};
const char *lframeskip[] = {"Auto","Off","1","2","3","4",NULL};
const char *lsdl_showfps[] = {"Off","Text Only","Boxed Text",NULL};
#if WIZ
const char *lclockspeeds[] = {"Default","250 mhz","300 mhz","350 mhz","400 mhz","450 mhz","500 mhz","550 mhz","600 mhz","650 mhz","700 mhz","750 mhz",NULL};
#endif
#ifdef DINGOO_NATIVE
const char *lclockspeeds[] = { "Default", "200 mhz", "250 mhz", "300 mhz", "336 mhz", "360 mhz", "400 mhz", NULL };
#endif
const char *volume_levels[] = {"0%", "10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%", "100%", NULL};
int volume_hardware = 100 / 10; /* todo make this an rc variable */


static char config[16][256];

int menu_options(){

	struct pal_s *palp=0;
	int pal=0, skip=0, ret=0, cfilter=0, sfps=0, upscale=0, speed=0, i=0;
	char *tmp=0, *romdir=0;

	FILE *file;
#ifdef DINGOO_NATIVE
    /*
    **  100Mhz once caused Dingoo A320 MIPS to hang,
    **  when 100Mhz worked BW GB (Adjustris) game was running at 32 fps (versus 60 at 200Mhz).
    **  150Mhz has never worked on my Dingoo A320.
    */
    uintptr_t dingoo_clock_speeds[] = { 200000000, 250000000, 300000000, 336000000, 360000000, 400000000 /* , 430000000 Should not be needed */ };
    /*
    ** under-under clock option is for GB games.
    ** GB games can often be ran under the already
    ** underclocked Dingoo speed of 336Mhz
    */

    bool dingoo_clock_change_result;
	uintptr_t tempCore=336000000; /* default Dingoo A320 clock speed */
	uintptr_t tempMemory=112000000; /* default Dingoo A320 memory speed */
	cpu_clock_get(&tempCore, &tempMemory);
#endif /* DINGOO_NATIVE */

	pal = findpal();
	cfilter = rc_getint("colorfilter");
	if(cfilter && !rc_getint("filterdmg")) cfilter = 2;
	upscale = rc_getint("upscaler");
	skip = rc_getint("frameskip")+1;
	sfps = rc_getint("showfps");
#ifdef DINGOO_NATIVE
	speed = 0;
#else
	speed = rc_getint("cpuspeed")/50 - 4;
#endif /* DINGOO_NATIVE */
	if(speed<0) speed = 0;
	if(speed>11) speed = 11;

	romdir = rc_getstr("romdir");
	romdir = romdir ? strdup(romdir) : strdup(".");

	start:

	dialog_begin("Options",NULL);

	dialog_option("Mono Palette",lpalettes,&pal);               /* 1 */
	dialog_option("Color Filter",lcolorfilter,&cfilter);        /* 2 */
	dialog_option("Upscaler",lupscaler,&upscale);               /* 3 */
	dialog_option("Frameskip",lframeskip,&skip);                /* 4 */
	dialog_option("Show FPS",lsdl_showfps,&sfps);               /* 5 */
#if defined(WIZ) || defined(DINGOO_NATIVE)
	dialog_option("Clock Speed",lclockspeeds,&speed);           /* 6 */
#else
	dialog_text("Clock Speed","Default",0);                     /* 6 */
#endif
	dialog_text("Rom Path",romdir,FIELD_SELECTABLE);            /* 7 */
	#ifdef GNUBOY_HARDWARE_VOLUME
	dialog_option("Volume", volume_levels, &volume_hardware);   /* 8 */ /* this is not the OSD volume.. */
	#else
	dialog_text("Volume", "Default", 0);                        /* 8 */ /* this is not the OSD volume.. */
	#endif /* GNBOY_HARDWARE_VOLUME */
	dialog_text(NULL,NULL,0);                                   /* 9 */
	dialog_text("Apply",NULL,FIELD_SELECTABLE);                 /* 10 */
	dialog_text("Apply & Save",NULL,FIELD_SELECTABLE);          /* 11 */
	dialog_text("Cancel",NULL,FIELD_SELECTABLE);                /* 12 */

	switch(ret=dialog_end()){
		case 7: /* "Rom Path" romdir */
			tmp = menu_requestdir("Select Rom Directory",romdir);
			if(tmp){
				free(romdir);
				romdir = tmp;
			}
			goto start;
		case 12: /* Cancel */
			return ret;
			break;
		case 10: /* Apply */
		case 11: /* Apply & Save */
			#ifdef GNUBOY_HARDWARE_VOLUME
			pcm_volume(volume_hardware * 10);
			#endif /* GNBOY_HARDWARE_VOLUME */
			palp = &gbpal[pal];
			if(speed)
			{
#ifdef DINGOO_NATIVE
                /*
                ** For now do NOT plug in into settings system, current
                ** (Wiz) speed system is focused on multiples of 50Mhz.
                ** Dingoo default clock speed is 336Mhz (CPU certified for
                ** 360, 433MHz is supposed to be possible).
                ** Only set clock speed if changed in options each and
                ** everytime - do not use config file
                */
                --speed;
                /* check menu response is withing the preset array range/size */
                if (speed > (sizeof(dingoo_clock_speeds)/sizeof(uintptr_t) - 1) )
                    speed = 0;
                
                tempCore = dingoo_clock_speeds[speed];
                dingoo_clock_change_result = cpu_clock_set(tempCore);
                
                tempCore=tempMemory=0;
                cpu_clock_get(&tempCore, &tempMemory); /* currently unused */
                /* TODO display clock speed next to on screen FPS indicator */
#endif /* DINGOO_NATIVE */
    
				speed = speed*50 + 200;
			}
			sprintf(config[0],"set dmg_bgp 0x%.6x 0x%.6x 0x%.6x 0x%.6x", palp->dmg_bgp[0], palp->dmg_bgp[1], palp->dmg_bgp[2], palp->dmg_bgp[3]);
			sprintf(config[1],"set dmg_wndp 0x%.6x 0x%.6x 0x%.6x 0x%.6x",palp->dmg_wndp[0],palp->dmg_wndp[1],palp->dmg_wndp[2],palp->dmg_wndp[3]);
			sprintf(config[2],"set dmg_obp0 0x%.6x 0x%.6x 0x%.6x 0x%.6x",palp->dmg_obp0[0],palp->dmg_obp0[1],palp->dmg_obp0[2],palp->dmg_obp0[3]);
			sprintf(config[3],"set dmg_obp1 0x%.6x 0x%.6x 0x%.6x 0x%.6x",palp->dmg_obp1[0],palp->dmg_obp1[1],palp->dmg_obp1[2],palp->dmg_obp1[3]);
			sprintf(config[4],"set colorfilter %i",cfilter!=0);
			sprintf(config[5],"set filterdmg %i",cfilter==1);
			sprintf(config[6],"set upscaler %i",upscale);
			sprintf(config[7],"set frameskip %i",skip-1);
			sprintf(config[8],"set showfps %i",sfps);
			sprintf(config[9],"set cpuspeed %i",speed);
			#ifdef DINGOO_NATIVE /* FIXME Windows too..... if (DIRSEP_CHAR == '\\').... */
			{
				char tmp_path[PATH_MAX];
				char *dest, *src;
				dest = &tmp_path[0];
				src = romdir;
				
				/* escape the path seperator (should escape other things too.) */
				while(*dest = *src++)
				{
					if (*dest == DIRSEP_CHAR)
					{
						dest++;
						*dest = DIRSEP_CHAR;
					}
					dest++;
				}
			
				sprintf(config[10], "set romdir \"%s\"", tmp_path);
				scaler_init(0);
			}
			#else
			sprintf(config[10],"set romdir \"%s\"",romdir);
			scaler_init(0);
			#endif /* DINGOO_NATIVE */

			for(i=0; i<11; i++)
				rc_command(config[i]);

			pal_dirty();

			if (ret == 11){ /* Apply & Save */
				file = fopen("ohboy.rc","w");
				for(i=0; i<11; i++){
					fputs(config[i],file);
					fputs("\n",file);
				}
				fclose(file);
			}

		break;
	}

	free(romdir);

	return ret;
}

int menu_controls(){

	int ret=0, i=0, btna=2, btnb=1, btnx=2, btny=1, btnl=3, btnr=4;

	FILE *file;

	btna = rc_getint("button_a");
	btnb = rc_getint("button_b");
	btnx = rc_getint("button_x");
	btny = rc_getint("button_y");
	btnl = rc_getint("button_l");
	btnr = rc_getint("button_r");

	start:

	dialog_begin("Controls",NULL);

	#if defined(CAANOO)
	dialog_text("Caanoo","Gameboy",0);                       /* 1 */
	#endif
	#if defined(WIZ)
	dialog_text("Wiz","Gameboy",0);                          /* 1 */
	#endif
	#if defined(DINGOO_NATIVE)
	dialog_text("Dingoo","Gameboy",0);                       /* 1 */
	#endif
	#if defined(GP2X_ONLY)
	dialog_text("GP2X","Gameboy",0);                         /* 1 */
	#endif
	dialog_text(NULL,NULL,0);                                /* 2 */
	dialog_option("Button A",lbutton_a,&btna);               /* 3 */
	dialog_option("Button B",lbutton_b,&btnb);               /* 4 */
	dialog_option("Button X",lbutton_x,&btnx);               /* 5 */
	dialog_option("Button Y",lbutton_y,&btny);               /* 6 */
	dialog_option("Button L",lbutton_l,&btnl);               /* 7 */
	dialog_option("Button R",lbutton_r,&btnr);               /* 8 */
	dialog_text(NULL,NULL,0);                                /* 9 */
	dialog_text("Apply",NULL,FIELD_SELECTABLE);              /* 10 */
	dialog_text("Apply & Save",NULL,FIELD_SELECTABLE);       /* 11 */
	dialog_text("Cancel",NULL,FIELD_SELECTABLE);             /* 12 */

	switch(ret=dialog_end()){
		case 12: /* Cancel */
			return ret;
			break;
		case 10: /* Apply */
		case 11: /* Apply & Save */
		    sprintf(config[0],"#KEY BINDINGS#");
			sprintf(config[1],"set button_a %i",btna);
			sprintf(config[2],"set button_b %i",btnb);
			sprintf(config[3],"set button_x %i",btnx);
			sprintf(config[4],"set button_y %i",btny);
			sprintf(config[5],"set button_l %i",btnl);
			sprintf(config[6],"set button_r %i",btnr);
			#if defined(CAANOO)
			    if (btna == 0) {sprintf(config[7],"unbind joy0");}
			    if (btnb == 0) {sprintf(config[8],"unbind joy2");}
			    if (btnx == 0) {sprintf(config[9],"unbind joy1");}
			    if (btny == 0) {sprintf(config[10],"unbind joy3");}
			    if (btnl == 0) {sprintf(config[11],"unbind joy4");}
			    if (btnr == 0) {sprintf(config[12],"unbind joy5");}

			    if (btna == 1) {sprintf(config[7],"bind joy0 +a");}
			    if (btnb == 1) {sprintf(config[8],"bind joy2 +a");}
			    if (btnx == 1) {sprintf(config[9],"bind joy1 +a");}
			    if (btny == 1) {sprintf(config[10],"bind joy3 +a");}
			    if (btnl == 1) {sprintf(config[11],"bind joy4 +a");}
			    if (btnr == 1) {sprintf(config[12],"bind joy5 +a");}

			    if (btna == 2) {sprintf(config[7],"bind joy0 +b");}
			    if (btnb == 2) {sprintf(config[8],"bind joy2 +b");}
			    if (btnx == 2) {sprintf(config[9],"bind joy1 +b");}
			    if (btny == 2) {sprintf(config[10],"bind joy3 +b");}
			    if (btnl == 2) {sprintf(config[11],"bind joy4 +b");}
			    if (btnr == 2) {sprintf(config[12],"bind joy5 +b");}

			    if (btna == 3) {sprintf(config[7],"bind joy0 +select");}
			    if (btnb == 3) {sprintf(config[8],"bind joy2 +select");}
			    if (btnx == 3) {sprintf(config[9],"bind joy1 +select");}
			    if (btny == 3) {sprintf(config[10],"bind joy3 +select");}
			    if (btnl == 3) {sprintf(config[11],"bind joy4 +select");}
			    if (btnr == 3) {sprintf(config[12],"bind joy5 +select");}

			    if (btna == 4) {sprintf(config[7],"bind joy0 +start");}
			    if (btnb == 4) {sprintf(config[8],"bind joy2 +start");}
			    if (btnx == 4) {sprintf(config[9],"bind joy1 +start");}
			    if (btny == 4) {sprintf(config[10],"bind joy3 +start");}
			    if (btnl == 4) {sprintf(config[11],"bind joy4 +start");}
			    if (btnr == 4) {sprintf(config[12],"bind joy5 +start");}

			    if (btna == 5) {sprintf(config[7],"bind joy0 reset");}
			    if (btnb == 5) {sprintf(config[8],"bind joy2 reset");}
			    if (btnx == 5) {sprintf(config[9],"bind joy1 reset");}
			    if (btny == 5) {sprintf(config[10],"bind joy3 reset");}
			    if (btnl == 5) {sprintf(config[11],"bind joy4 reset");}
			    if (btnr == 5) {sprintf(config[12],"bind joy5 reset");}

			    if (btna == 6) {sprintf(config[7],"bind joy0 quit");}
			    if (btnb == 6) {sprintf(config[8],"bind joy2 quit");}
			    if (btnx == 6) {sprintf(config[9],"bind joy1 quit");}
			    if (btny == 6) {sprintf(config[10],"bind joy3 quit");}
			    if (btnl == 6) {sprintf(config[11],"bind joy4 quit");}
			    if (btnr == 6) {sprintf(config[12],"bind joy5 quit");}
			#endif
			#if defined(DINGOO_NATIVE)
			    if (btna == 0) {sprintf(config[7],"unbind ctrl");}
			    if (btnb == 0) {sprintf(config[8],"unbind alt");}
			    if (btnx == 0) {sprintf(config[9],"unbind space");}
			    if (btny == 0) {sprintf(config[10],"unbind shift");}
			    if (btnl == 0) {sprintf(config[11],"unbind tab");}
			    if (btnr == 0) {sprintf(config[12],"unbind backspace");}

			    if (btna == 1) {sprintf(config[7],"bind ctrl +a");}
			    if (btnb == 1) {sprintf(config[8],"bind alt +a");}
			    if (btnx == 1) {sprintf(config[9],"bind space +a");}
			    if (btny == 1) {sprintf(config[10],"bind shift +a");}
			    if (btnl == 1) {sprintf(config[11],"bind tab +a");}
			    if (btnr == 1) {sprintf(config[12],"bind backspace +a");}

			    if (btna == 2) {sprintf(config[7],"bind ctrl +b");}
			    if (btnb == 2) {sprintf(config[8],"bind alt +b");}
			    if (btnx == 2) {sprintf(config[9],"bind space +b");}
			    if (btny == 2) {sprintf(config[10],"bind shift +b");}
			    if (btnl == 2) {sprintf(config[11],"bind tab +b");}
			    if (btnr == 2) {sprintf(config[12],"bind backspace +b");}

			    if (btna == 3) {sprintf(config[7],"bind ctrl +select");}
			    if (btnb == 3) {sprintf(config[8],"bind alt +select");}
			    if (btnx == 3) {sprintf(config[9],"bind space +select");}
			    if (btny == 3) {sprintf(config[10],"bind shift +select");}
			    if (btnl == 3) {sprintf(config[11],"bind tab +select");}
			    if (btnr == 3) {sprintf(config[12],"bind backspace +select");}

			    if (btna == 4) {sprintf(config[7],"bind ctrl +start");}
			    if (btnb == 4) {sprintf(config[8],"bind alt +start");}
			    if (btnx == 4) {sprintf(config[9],"bind space +start");}
			    if (btny == 4) {sprintf(config[10],"bind shift +start");}
			    if (btnl == 4) {sprintf(config[11],"bind tab +start");}
			    if (btnr == 4) {sprintf(config[12],"bind backspace +start");}

			    if (btna == 5) {sprintf(config[7],"bind ctrl reset");}
			    if (btnb == 5) {sprintf(config[8],"bind alt reset");}
			    if (btnx == 5) {sprintf(config[9],"bind space reset");}
			    if (btny == 5) {sprintf(config[10],"bind shift reset");}
			    if (btnl == 5) {sprintf(config[11],"bind tab reset");}
			    if (btnr == 5) {sprintf(config[12],"bind backspace reset");}

			    if (btna == 6) {sprintf(config[7],"bind ctrl quit");}
			    if (btnb == 6) {sprintf(config[8],"bind alt quit");}
			    if (btnx == 6) {sprintf(config[9],"bind space quit");}
			    if (btny == 6) {sprintf(config[10],"bind shift quit");}
			    if (btnl == 6) {sprintf(config[11],"bind tab quit");}
			    if (btnr == 6) {sprintf(config[12],"bind backspace quit");}
			#endif
			#if defined(WIZ) || defined(GP2X_ONLY)
			    if (btna == 0) {sprintf(config[7],"unbind joy12");}
			    if (btnb == 0) {sprintf(config[8],"unbind joy13");}
			    if (btnx == 0) {sprintf(config[9],"unbind joy14");}
			    if (btny == 0) {sprintf(config[10],"unbind joy15");}
			    if (btnl == 0) {sprintf(config[11],"unbind joy10");}
			    if (btnr == 0) {sprintf(config[12],"unbind joy11");}

			    if (btna == 1) {sprintf(config[7],"bind joy12 +a");}
			    if (btnb == 1) {sprintf(config[8],"bind joy13 +a");}
			    if (btnx == 1) {sprintf(config[9],"bind joy14 +a");}
			    if (btny == 1) {sprintf(config[10],"bind joy15 +a");}
			    if (btnl == 1) {sprintf(config[11],"bind joy10 +a");}
			    if (btnr == 1) {sprintf(config[12],"bind joy11 +a");}

			    if (btna == 2) {sprintf(config[7],"bind joy12 +b");}
			    if (btnb == 2) {sprintf(config[8],"bind joy13 +b");}
			    if (btnx == 2) {sprintf(config[9],"bind joy14 +b");}
			    if (btny == 2) {sprintf(config[10],"bind joy15 +b");}
			    if (btnl == 2) {sprintf(config[11],"bind joy10 +b");}
			    if (btnr == 2) {sprintf(config[12],"bind joy11 +b");}

			    if (btna == 3) {sprintf(config[7],"bind joy12 +select");}
			    if (btnb == 3) {sprintf(config[8],"bind joy13 +select");}
			    if (btnx == 3) {sprintf(config[9],"bind joy14 +select");}
			    if (btny == 3) {sprintf(config[10],"bind joy15 +select");}
			    if (btnl == 3) {sprintf(config[11],"bind joy10 +select");}
			    if (btnr == 3) {sprintf(config[12],"bind joy11 +select");}

			    if (btna == 4) {sprintf(config[7],"bind joy12 +start");}
			    if (btnb == 4) {sprintf(config[8],"bind joy13 +start");}
			    if (btnx == 4) {sprintf(config[9],"bind joy14 +start");}
			    if (btny == 4) {sprintf(config[10],"bind joy15 +start");}
			    if (btnl == 4) {sprintf(config[11],"bind joy10 +start");}
			    if (btnr == 4) {sprintf(config[12],"bind joy11 +start");}

			    if (btna == 5) {sprintf(config[7],"bind joy12 reset");}
			    if (btnb == 5) {sprintf(config[8],"bind joy13 reset");}
			    if (btnx == 5) {sprintf(config[9],"bind joy14 reset");}
			    if (btny == 5) {sprintf(config[10],"bind joy15 reset");}
			    if (btnl == 5) {sprintf(config[11],"bind joy10 reset");}
			    if (btnr == 5) {sprintf(config[12],"bind joy11 reset");}

			    if (btna == 6) {sprintf(config[7],"bind joy12 quit");}
			    if (btnb == 6) {sprintf(config[8],"bind joy13 quit");}
			    if (btnx == 6) {sprintf(config[9],"bind joy14 quit");}
			    if (btny == 6) {sprintf(config[10],"bind joy15 quit");}
			    if (btnl == 6) {sprintf(config[11],"bind joy10 quit");}
			    if (btnr == 6) {sprintf(config[12],"bind joy11 quit");}
			#endif
			for(i=0; i<13; i++)
				rc_command(config[i]);

			pal_dirty();

			if (ret == 11){ /* Apply & Save */
				file = fopen("bindings.rc","w");
				for(i=0; i<13; i++){
					fputs(config[i],file);
					fputs("\n",file);
				}
				fclose(file);
			}
		break;
	}
	return ret;
}

int menu_about(){

int ret=0;

char version_str[80];
char ohboy_ver_str[80];

snprintf(version_str, sizeof(version_str)-1, "Gnuboy %s", VERSION);
snprintf(ohboy_ver_str, sizeof(ohboy_ver_str)-1, "%s", OHBOY_VER);

	start:

	dialog_begin("About OhBoy",NULL);

	dialog_text(NULL,"OhBoy is a Gameboy emulator for",0);
	dialog_text(NULL,"small handhelds, using the",0);
	dialog_text(NULL,"Gnuboy emulation core with a",0);
	dialog_text(NULL,"basic menu system.",0);
	dialog_text(NULL,NULL,0);
	dialog_text(NULL,"OhBoy version:",0);
	dialog_text(NULL,ohboy_ver_str,0);
	dialog_text(NULL,NULL,0);
	dialog_text(NULL,"Using:",0);
	dialog_text(NULL,version_str,0);
	dialog_text(NULL,NULL,0);
	dialog_text(NULL,"More info:",0);
	dialog_text(NULL,"http://ohboy.googlecode.com/",0);
	dialog_text(NULL,"http://gnuboy.googlecode.com/",0);
	dialog_text(NULL,NULL,FIELD_SELECTABLE); /* Not visible, but always selected, allows to exit to main menu by pressing the "selection" button. You can also go to main menu pressing the "back" button */

	switch(ret=dialog_end()){
		case 1:
			return ret;
			break;
	}
	return ret;
}

int menu(){

	char *dir;
	int mexit=0;
	static char *loadrom;
	int old_upscale = 0, new_upscale = 0;

	old_upscale = rc_getint("upscaler");
	gui_begin();
	while(!mexit){
		dialog_begin(rom.name,"ohBoy");

		dialog_text("Back to Game",NULL,FIELD_SELECTABLE);
		dialog_text("Load State",NULL,FIELD_SELECTABLE);
		dialog_text("Save State",NULL,FIELD_SELECTABLE);
		dialog_text("Reset Game",NULL,FIELD_SELECTABLE);
		dialog_text(NULL,NULL,0);
		dialog_text("Load ROM",NULL,FIELD_SELECTABLE);
		dialog_text("Options",NULL,FIELD_SELECTABLE);
		dialog_text("Controls",NULL,FIELD_SELECTABLE);
		dialog_text("About",NULL,FIELD_SELECTABLE);
		dialog_text("Quit","",FIELD_SELECTABLE);
		
#ifdef DINGOO_NATIVE
		dialog_text(NULL, NULL, 0); /* blank line */
		dialog_text("Menu:", NULL, 0);
		dialog_text(" Slide Power", NULL, 0);
#endif /* DINGOO_NATIVE */

		switch(dialog_end()){
			case 2:
				if(menu_state(0)) mexit=1;
				break;
			case 3:
				if(menu_state(1)) mexit=1;
				break;
			case 4:
				rc_command("reset");
				mexit=1;
				break;
			case 6:
				dir = rc_getstr("romdir");
				if(loadrom = menu_requestfile(NULL,"Select Rom",dir,"gb;gbc;zip")) {
					loader_unload();
					ohb_loadrom(loadrom);
					mexit=1;
				}
				break;
			case 7:
				if(menu_options()) mexit=1;
				break;
			case 8:
				if(menu_controls()) mexit=1;
				break;
			case 9:
				if(menu_about()) mexit=0;
				break;
			case 10:
				exit(0);
				break;
			default:
				mexit=1;
				break;
		}
	}
	new_upscale = rc_getint("upscaler");
	if (old_upscale != new_upscale)
		scaler_init(new_upscale);
	gui_end();

	return 0;
}

/*#include VERSION*/

int launcher(){;

	char *rom = 0;
	char *dir = rc_getstr("romdir");
	char version_str[80];
    
    snprintf(version_str, sizeof(version_str)-1, "gnuboy %s", VERSION);

	gui_begin();

launcher:
	dialog_begin("OhBoy http://ohboy.googlecode.com/", version_str);
	dialog_text("Load ROM",NULL,FIELD_SELECTABLE);
	dialog_text("Options",NULL,FIELD_SELECTABLE);
	dialog_text("Controls",NULL,FIELD_SELECTABLE);
	dialog_text("About",NULL,FIELD_SELECTABLE);
	dialog_text("Quit","",FIELD_SELECTABLE);

#ifdef DINGOO_NATIVE
	dialog_text(NULL, NULL, 0); /* blank line */
	dialog_text("Menu:", NULL, 0);
	dialog_text(" Slide Power", NULL, 0);
#endif /* DINGOO_NATIVE */

	switch(dialog_end()){
		case 1:
			rom = menu_requestfile(NULL,"Select Rom",dir,"gb;gbc;zip");
			if(!rom) goto launcher;
			break;
		case 2:
			if(!menu_options()) goto launcher;
			break;
		case 3:
			if(!menu_controls()) goto launcher;
			break;
		case 4:
			if(!menu_about()) goto launcher;
			break;
		case 5:
			exit(0);
		default:
			goto launcher;
	}

	gui_end();

	return rom;
}

