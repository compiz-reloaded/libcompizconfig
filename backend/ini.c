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
#include <dirent.h>
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
	char *fileName = NULL;

	homeDir = getenv ("HOME");

	if (!homeDir)
		return NULL;

	asprintf (&fileName, "%s/%s%s.ini", homeDir, SETTINGPATH, profile);

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
	char *keyName;

	asprintf (&keyName, "%s:%s", setting->parent->name, setting->name);

	switch (setting->type)
	{
	    case TypeString:
			{
				char *value;
				value = iniparser_getstring (iniFile, keyName, NULL);

				if (value)
				{
					bsSetString (setting, value);
					status = TRUE;
				}
			}
			break;
		case TypeMatch:
			{
				char *value;
				value = iniparser_getstring (iniFile, keyName, NULL);

				if (value)
				{
					bsSetMatch (setting, value);
					status = TRUE;
				}
			}
			break;
		case TypeInt:
			{
				int value;
				value = iniparser_getint (iniFile, keyName, 0x7fffffff);

				if (value != 0x7fffffff)
				{
					bsSetInt (setting, value);
					status = TRUE;
				}
			}
			break;
		case TypeBool:
			{
				int value;
				value = iniparser_getboolean (iniFile, keyName, 0x7fffffff);

				if (value != 0x7fffffff)
				{
					bsSetBool (setting, (value != 0));
					status = TRUE;
				}
			}
			break;
		case TypeFloat:
			{
				float value;
				value = (float) iniparser_getdouble (iniFile, keyName, 0x7ffffffff);

				if (value != 0x7ffffffff)
				{
					bsSetFloat (setting, value);
					status = TRUE;
				}
			}
			break;
		case TypeColor:
			break;
		case TypeList:
			break;
		case TypeAction:
			break;
		default:
			break;
	}

	if (status)
	{
		if (strcmp(setting->name, "____plugin_enabled") == 0)
			context->pluginsChanged = TRUE;
		context->changedSettings = bsSettingListAppend(context->changedSettings, setting);
	}

	if (keyName)
		free (keyName);
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
	char *keyName;

	asprintf (&keyName, "%s:%s", setting->parent->name, setting->name);

	switch (setting->type)
	{
		case TypeString:
			{
				char *value;
				if (bsGetString (setting, &value))
					iniparser_setstr (iniFile, keyName, value);
			}
			break;
		case TypeMatch:
			{
				char *value;
				if (bsGetMatch (setting, &value))
					iniparser_setstr (iniFile, keyName, value);
			}
			break;
		case TypeInt:
			{
				int value;
				if (bsGetInt (setting, &value))
				{
					char string[64]; // should be enough for an int
					snprintf (string, 64, "%i", value);
					iniparser_setstr (iniFile, keyName, string);
				}
			}
			break;
		case TypeFloat:
			{
				float value;
				if (bsGetFloat (setting, &value))
				{
					char string[64];
					snprintf (string, 64, "%f", value);
					iniparser_setstr (iniFile, keyName, string);
				}
			}
			break;
		case TypeBool:
			{
				Bool value;
				if (bsGetBool (setting, &value))
					iniparser_setstr (iniFile, keyName, value ? "true" : "false");
			}
			break;
		case TypeColor:
			break;
		case TypeAction:
			break;
		case TypeList:
			break;
		default:
			break;
	}

	if (keyName)
		free (keyName);
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

static int profileNameFilter (const struct dirent *name)
{
	int length = strlen (name->d_name);

	if (strncmp (name->d_name + length - 4, ".ini", 4))
		return 0;

	return 1;
}

static BSStringList getExistingProfiles(void)
{
	BSStringList  ret = NULL;
	struct dirent **nameList;
	char          *homeDir = NULL;
	char          *filePath = NULL;
   	char          *pos;
	int           nFile, i;

	homeDir = getenv ("HOME");
	if (!homeDir)
		return NULL;

	asprintf (&filePath, "%s/%s", homeDir, SETTINGPATH);
	if (!filePath)
		return NULL;

	nFile = scandir(filePath, &nameList, profileNameFilter, NULL);

	if (nFile <= 0)
		return;

	for (i = 0; i < nFile; i++)
	{
		pos = strrchr (nameList[i]->d_name);
		if (pos)
		{
			*pos = 0;
			ret = bsStringListAppend (ret, nameList[i]->d_name);
		}
		free(nameList[i]);
	}

	free (filePath);
	free (nameList);

	return ret;
}

static Bool deleteProfile(char * profile)
{
	char *fileName;

	fileName = getIniFileName (profile);
	if (!fileName)
		return FALSE;

	remove (fileName);
	free (fileName);

	return TRUE;
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

