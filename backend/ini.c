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

#include <sys/inotify.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>

#include <bsettings.h>

#include <X11/X.h>
#include <X11/Xlib.h>

#define DEFAULTPROF "Default"
#define SETTINGPATH ".bsettings/"

typedef struct _IniPrivData
{
	BSContext *context;
	char * lastProfile;
	IniDictionary * iniFile;
	unsigned int iniWatchId;
} IniPrivData;

static IniPrivData *privData = NULL;
static int privDataSize = 0;

/* forward declaration */
static void setProfile(IniPrivData *data, char *profile);

static IniPrivData *findPrivFromContext (BSContext *context)
{
	int i;
	IniPrivData *data;

	for (i = 0, data = privData; i < privDataSize; i++, data++)
		if (data->context == context)
			break;

	if (i == privDataSize)
		return NULL;

	return data;
}

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

static void processFileEvent(unsigned int watchId, void *closure)
{
	IniPrivData *data = (IniPrivData *)closure;
	char *fileName;

	/* our ini file has been modified, reload it */

	if (data->iniFile)
		bsIniClose(data->iniFile);

	fileName = getIniFileName (data->lastProfile);
	if (!fileName)
		return;

	data->iniFile = bsIniOpen(fileName);

	bsReadSettings (data->context);
}

static void setProfile(IniPrivData *data, char *profile)
{
	char *fileName;
	struct stat fileStat;

	if (data->iniFile)
		bsIniClose (data->iniFile);
	if (data->iniWatchId)
		bsRemoveFileWatch (data->iniWatchId);

	data->iniFile = NULL;
	data->iniWatchId = 0;

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

	data->iniWatchId = bsAddFileWatch (fileName, TRUE, processFileEvent, data);

	/* load the data from the file */
	data->iniFile = bsIniOpen (fileName);

	free (fileName);
}

static Bool initBackend(BSContext * context)
{
	IniPrivData *newData;

	privData = realloc (privData, (privDataSize + 1) * sizeof(IniPrivData));
	newData = privData + privDataSize;

	/* initialize the newly allocated part */
	memset(newData, 0, sizeof(IniPrivData));
	newData->context = context;

	privDataSize++;

	return TRUE;
}

static Bool finiBackend(BSContext * context)
{
	IniPrivData *data;

	data = findPrivFromContext (context);

	if (!data)
		return FALSE;

	if (data->iniFile)
		bsIniClose (data->iniFile);

	if (data->lastProfile)
		free (data->lastProfile);

	privDataSize--;

	if (privDataSize)
		privData = realloc (privData, privDataSize * sizeof(IniPrivData));
	else
		free (privData);

	return TRUE;
}

static Bool readInit(BSContext * context)
{
	char *currentProfile;
	IniPrivData *data;

	data = findPrivFromContext (context);
	if (!data)
		return FALSE;
	
	currentProfile = bsGetProfile(context);
	if (!currentProfile || !strlen(currentProfile))
		currentProfile = strdup (DEFAULTPROF);
	else
		currentProfile = strdup (currentProfile);

	if (!data->lastProfile || (strcmp(data->lastProfile, currentProfile) != 0))
		setProfile (data, currentProfile);

	if (data->lastProfile)
		free (data->lastProfile);

	data->lastProfile = currentProfile;

	return (data->iniFile != NULL);
}

static void readSetting(BSContext * context, BSSetting * setting)
{
	Bool status = FALSE;
	char *keyName;
	IniPrivData *data;

	data = findPrivFromContext (context);
	if (!data)
		return;

	if (setting->isScreen)
		asprintf (&keyName, "s%d_%s", setting->screenNum, setting->name);
	else
		asprintf (&keyName, "as_%s", setting->name);

	switch (setting->type)
	{
	    case TypeString:
			{
				char *value;
				if (bsIniGetString (data->iniFile, setting->parent->name,
									keyName, &value))
				{
					bsSetString (setting, value);
					status = TRUE;
				}
			}
			break;
		case TypeMatch:
			{
				char *value;
				if (bsIniGetString (data->iniFile, setting->parent->name,
									keyName, &value))
				{
					bsSetMatch (setting, value);
					status = TRUE;
				}
			}
			break;
		case TypeInt:
			{
				int value;
				if (bsIniGetInt (data->iniFile, setting->parent->name,
								 keyName, &value))
				{
					bsSetInt (setting, value);
					status = TRUE;
				}
			}
			break;
		case TypeBool:
			{
				Bool value;

				if (bsIniGetBool (data->iniFile, setting->parent->name,
								  keyName, &value))
				{
					bsSetBool (setting, (value != 0));
					status = TRUE;
				}
			}
			break;
		case TypeFloat:
			{
				float value;
				if (bsIniGetFloat (data->iniFile, setting->parent->name,
								   keyName, &value))
				{
					bsSetFloat (setting, value);
					status = TRUE;
				}
			}
			break;
		case TypeColor:
			{
				BSSettingColorValue color;
				if (bsIniGetColor (data->iniFile, setting->parent->name,
								   keyName, &color))
				{
					bsSetColor (setting, color);
					status = TRUE;
				}
			}
			break;
		case TypeAction:
			{
				BSSettingActionValue action;
				if (bsIniGetAction (data->iniFile, setting->parent->name,
									keyName, &action))
				{
					bsSetAction (setting, action);
					status = TRUE;
				}
			}
			break;
		case TypeList:
			{
				BSSettingValueList value;
				if (bsIniGetList (data->iniFile, setting->parent->name,
								  keyName, &value, setting))
				{
					bsSetList (setting, value);
					bsSettingValueListFree (value, TRUE);
					status = TRUE;
				}
			}
			break;
		default:
			break;
	}

	if (!status)
	{
		/* reset setting to default if it could not be read */
		bsResetToDefault (setting);
	}

	if (keyName)
		free (keyName);
}

static void readDone(BSContext * context)
{
}

static Bool writeInit(BSContext * context)
{
	char *currentProfile;
	IniPrivData *data;

	data = findPrivFromContext (context);
	if (!data)
		return FALSE;

	currentProfile = bsGetProfile (context);
	if (!currentProfile || !strlen(currentProfile))
		currentProfile = strdup (DEFAULTPROF);
	else
		currentProfile = strdup (currentProfile);

	if (!data->lastProfile || (strcmp(data->lastProfile, currentProfile) != 0))
		setProfile (data, currentProfile);

	if (data->lastProfile)
		free (data->lastProfile);

	bsDisableFileWatch (data->iniWatchId);

	data->lastProfile = currentProfile;

	return (data->iniFile != NULL);
}

static void writeSetting(BSContext * context, BSSetting * setting)
{
	char *keyName;
	IniPrivData *data;

	data = findPrivFromContext (context);
	if (!data)
		return;

	if (setting->isScreen)
		asprintf (&keyName, "s%d_%s", setting->screenNum, setting->name);
	else
		asprintf (&keyName, "as_%s", setting->name);

	if (setting->isDefault)
	{
		bsIniRemoveEntry (data->iniFile, setting->parent->name, keyName);
		free (keyName);
		return;
	}

	switch (setting->type)
	{
		case TypeString:
			{
				char *value;
				if (bsGetString (setting, &value))
					bsIniSetString (data->iniFile, setting->parent->name,
									keyName, value);
			}
			break;
		case TypeMatch:
			{
				char *value;
				if (bsGetMatch (setting, &value))
					bsIniSetString (data->iniFile, setting->parent->name,
									keyName, value);
			}
			break;
		case TypeInt:
			{
				int value;
				if (bsGetInt (setting, &value))
					bsIniSetInt (data->iniFile, setting->parent->name,
								 keyName, value);
			}
			break;
		case TypeFloat:
			{
				float value;
				if (bsGetFloat (setting, &value))
					bsIniSetFloat (data->iniFile, setting->parent->name,
								   keyName, value);
			}
			break;
		case TypeBool:
			{
				Bool value;
				if (bsGetBool (setting, &value))
					bsIniSetBool (data->iniFile, setting->parent->name,
								  keyName, value);
			}
			break;
		case TypeColor:
			{
				BSSettingColorValue value;
				if (bsGetColor (setting, &value))
					bsIniSetColor (data->iniFile, setting->parent->name,
								   keyName, value);
			}
			break;
		case TypeAction:
			{
				BSSettingActionValue value;
				if (bsGetAction (setting, &value))
					bsIniSetAction (data->iniFile, setting->parent->name,
									keyName, value);
			}
			break;
		case TypeList:
			{
				BSSettingValueList value;
				if (bsGetList (setting, &value))
					bsIniSetList (data->iniFile, setting->parent->name,
								  keyName, value, setting->info.forList.listType);
			}
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
	char *fileName;
	char *currentProfile;
	IniPrivData *data;

	data = findPrivFromContext (context);
	if (!data)
		return;

	currentProfile = bsGetProfile (context);
	if (!currentProfile || !strlen(currentProfile))
		currentProfile = strdup (DEFAULTPROF);
	else
		currentProfile = strdup (currentProfile);

	fileName = getIniFileName (currentProfile);

	free (currentProfile);

	bsIniSave (data->iniFile, fileName);

	bsEnableFileWatch (data->iniWatchId);

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
		return NULL;

	for (i = 0; i < nFile; i++)
	{
		pos = strrchr (nameList[i]->d_name, '.');
		if (pos)
		{
			*pos = 0;
			if (strcmp(nameList[i]->d_name, DEFAULTPROF) != 0) 
				ret = bsStringListAppend (ret, strdup(nameList[i]->d_name));
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
	NULL,
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

