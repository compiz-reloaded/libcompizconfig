/**
 *
 * INI bsettings backend
 *
 * ini.c
 *
 * Copyright (c) 2007 Danny Baumann <maniac@beryl-project.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 **/



#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include <bsettings.h>
#include "iniparser.h"
#include "dictionary.h"
#include "strlib.h"

#include <X11/X.h>
#include <X11/Xlib.h>

#define CompAltMask        (1 << 16)
#define CompMetaMask       (1 << 17)
#define CompSuperMask      (1 << 18)
#define CompHyperMask      (1 << 19)
#define CompModeSwitchMask (1 << 20)

#define CompNumLockMask    (1 << 21)
#define CompScrollLockMask (1 << 22)

#define DEFAULTPROF "Default"
#define SETTINGPATH ".bsettings/"

static char * currentProfile = NULL;
static char * lastProfile = NULL;
static dictionary * iniFile = NULL;

static char* getIniFileName(char *profile)
{
	char *homeDir = NULL;
	char *fileName;
	int  pathLen = 0;

	homeDir = getenv ("HOME");

	if (!homeDir)
		return NULL;

	pathLen = strlen (homeDir) + strlen (SETTINGPATH) + strlen (profile) + 6;
	fileName = malloc (pathLen * sizeof(char));

	snprintf (fileName, pathLen, "%s/%s%s.ini", homeDir, SETTINGPATH, profile);

	return fileName;
}

static void setProfile(char *profile)
{
	char *fileName;
	struct stat fileStat;

	if (iniFile)
		iniparser_freedict (iniFile);

	iniFile = NULL;

	/* first we need to find the file name */
	fileName = getIniFileName (profile);

	if (!fileName)
		return;

	/* if the file does not exist, we have to create it */
	if (stat (fileName, &fileStat) == -1)
	{
		if (errno == ENOENT)
		{
			FILE *file;
			file = fopen (fileName, "w");
			if (!file)
				return;
			fclose (file);
		}
		else
			return;
	}

	/* load the data from the file */
	iniFile = iniparser_load (fileName);

	free (fileName);
}

static void processEvents(void)
{
}

static Bool initBackend(BSContext * context)
{
	return FALSE;
}

static Bool finiBackend(BSContext * context)
{
	if (iniFile)
		iniparser_freedict (iniFile);
	iniFile = NULL;

	if (lastProfile)
		free (lastProfile);
	lastProfile = NULL;

	return TRUE;
}

static Bool readInit(BSContext * context)
{
	currentProfile = bsGetProfile(context);
	if (!currentProfile)
		currentProfile = DEFAULTPROF;

	if (!lastProfile || (strcmp(lastProfile, currentProfile) != 0))
		setProfile (currentProfile);

	if (lastProfile)
		free (lastProfile);

	if (!iniFile)
		return FALSE;

	lastProfile = strdup (currentProfile);
	
	return TRUE;
}

static void readSetting(BSContext * context, BSSetting * setting)
{
	Bool status = FALSE;

	if (status)
		if (strcmp(setting->name, "____plugin_enabled") == 0)
			context->pluginsChanged = TRUE;
		context->changedSettings = bsSettingListAppend(context->changedSettings, setting);
}

static void readDone(BSContext * context)
{
}

static Bool writeInit(BSContext * context)
{
	currentProfile = bsGetProfile(context);
	if (!currentProfile)
		currentProfile = DEFAULTPROF;

	if (!lastProfile || (strcmp(lastProfile, currentProfile) != 0))
		setProfile (currentProfile);

	if (lastProfile)
		free (lastProfile);

	if (!iniFile)
		return FALSE;

	lastProfile = strdup (currentProfile);
	
	return TRUE;
}

static void writeSetting(BSContext * context, BSSetting * setting)
{
}

static void writeDone(BSContext * context)
{
	/* export the data to ensure the changes are on disk */
	FILE *file;
	char *fileName;

	fileName = getIniFileName (currentProfile);

	file = fopen (fileName, "w");
	iniparser_dump_ini (iniFile, file);
	fclose (file);
	free (fileName);
}

static Bool getSettingIsReadOnly(BSSetting * setting)
{
	/* FIXME */
	return FALSE;
}

static BSStringList getExistingProfiles(void)
{
	BSStringList ret = NULL;

	return ret;
}

static Bool deleteProfile(char * profile)
{
	Bool status = FALSE;

	return status;
}


static BSBackendVTable iniVTable = {
    "ini",
    "INI Configuration Backend",
    "INI Configuration Backend for bsettings",
    FALSE,
    TRUE,
    processEvents,
    initBackend,
    finiBackend,
	readInit,
	readSetting,
	readDone,
	writeInit,
	writeSetting,
	writeDone,
	NULL,
	getSettingIsReadOnly,
	getExistingProfiles,
	deleteProfile
};

BSBackendVTable *
getBackendInfo (void)
{
    return &iniVTable;
}

