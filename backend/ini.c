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
#include "option.h"

#include <X11/X.h>
#include <X11/Xlib.h>

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

static Bool readActionValue (BSSettingActionValue * action, char * valueString)
{
	char *value, *valueStart;
	char *token;

	memset (action, 0, sizeof(BSSettingActionValue));
	value = strdup (valueString);
	valueStart = value;

	token = strsep (&value, ",");
	if (!token)
		goto invalidaction;

	/* key binding */
	stringToKeyBinding (token, action);

	token = strsep (&value, ",");
	if (!token)
		goto invalidaction;

	/* button binding */
	stringToButtonBinding (token, action);

	token = strsep (&value, ",");
	if (!token)
		goto invalidaction;

	/* edge binding */
	stringToEdge (token, action);

	token = strsep (&value, ",");
	if (!token)
		goto invalidaction;

	/* edge button */
	action->edgeButton = stroul (token, NULL, 10);

	/* bell */
	action->onBell = (strcmp (value, "true") == 0);

	free (valueStart);

	return TRUE;

invalidaction:
	free (valueStart);
	return FALSE;
}

static void writeActionValue (BSSettingActionValue * action, char * keyName)
{
	char *keyBinding;
	char *buttonBinding;
	char *edge;
	char *actionString;

	keyBinding = keyBindingToString(action);
	if (!keyBinding)
		keyBinding = strdup("");

	buttonBinding = buttonBindingToString(action);
	if (!buttonBinding)
		buttonBinding = strdup("");

	edge = edgeToString(action->edgeMask);
	if (!edge)
		edge = strdup("");

	asprintf (&actionString, "%s,%s,%s,%d,%s\n", keyBinding,
			  buttonBinding, edge, action->edgeButton,
			  action->onBell ? "true" : "false");

	iniparser_setstr (iniFile, keyName, actionString);
}

static Bool readListValue (BSSetting * setting, char * keyName)
{
	BSSettingValueList = NULL;
	char *value, *valueStart, *valString;
	char *token;
	int nItems = 0, i = 0;

	valString = iniparser_getstring (iniFile, keyName, NULL);
	if (!valString)
		return FALSE;

	value = strdup (valString);
	valueStart = value;

	while (token)
	{
		token = strsep (&value, ";");
		nItems++;
	}
	value = valueStart;

	token = strsep (&value, ";");

	switch (setting->info.forList.listType)
	{
		case TypeString:
		case TypeMatch:
			{
				char **array = malloc (nItems * sizeof(char*));
				while (token)
				{
					array[i++] = strdup (token);
					token = strsep (&value, ";");
				}
				list = bsGetValueListFromStringArray (array, nItems, setting);
				for (i = 0; i < nItems; i++)
					free (array[i]);
				free (array);
			}
			break;
		case TypeColor:
			{
				BSSettingColorValue *array = malloc (nItems * sizeof(BSSettingColorValue));
				while (token)
				{
                    memset(&array[i], 0, sizeof(BSSettingColorValue));
                   	stringToColor(token, &array[i]);
					token = strsep (&value, ";");
					i++;
				}
				list = bsGetValueListFromColorArray (array, nItems, setting);
				free (array);
			}
			break;
		case TypeBool:
			{
				Bool *array = malloc (nItems * sizeof(Bool));
				Bool isTrue;
				while (token)
				{
					isTrue = (token[0] == 'y' || token[0] == 'Y' || token[0] == '1' ||
							  token[0] == 't' || token[0] == 'T');
					array[i++] = isTrue;
					token = strsep (&value, ";");
				}
				list = bsGetValueListFromBoolArray (array, nItems, setting);
				free (array);
			}
			break;
		case TypeInt:
			{
				int *array = malloc (nItems * sizeof(int));
				while (token)
				{
					array[i++] = strtoul (token, NULL, 10);
					token = strsep (&value, ";");
				}
				list = bsGetValueListFromIntArray (array, nItems, setting);
				free (array);
			}
			break;
		case TypeFloat:
			{
				float *array = malloc (nItems * sizeof(float));
				while (token)
				{
					array[i++] = strtod (token, NULL);
					token = strsep (&value, ";");
				}
				list = bsGetValueListFromFloatArray (array, nItems, setting);
				free (array);
			}
			break;
		case TypeAction:
			{
				BSSettingActionValue *array = malloc (nItems * sizeof(BSSettingActionValue));
				while (token)
				{
					readActionValue (&array[i++], token);
					token = strsep (&value, ";");
				}
				list = bsGetValueListFromActionArray (array, nItems, setting);
				free (array);
			}
			break;
		default:
			break;
	}

	if (list)
	{
		bsSetList (setting, list);
		bsSettingValueListFree (list, TRUE);
		free (valueStart);
		return TRUE;
	}

	free (valueStart);

	return FALSE;
}

static void writeListValue (BSSetting * setting, char * keyName)
{

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

	if (setting->isScreen)
		asprintf (&keyName, "%s:s%d_%s", setting->parent->name, 
				  setting->screenNum, setting->name);
	else
		asprintf (&keyName, "%s:as_%s", setting->parent->name, setting->name);

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
			{
				char *value;
				BSSettingColorValue color;
				value = iniparser_getstring (iniFile, keyName, NULL);

				if (value && stringToColor (value, &color.array))
				{
					bsSetColor (setting, color);
					status = TRUE;
				}
			}
			break;
		case TypeAction:
			{
				BSSettingActionValue action;
				char *value;

				value = iniparser_getstring (iniFile, keyName, NULL);
				if (!value)
					break;
				
				if (readActionValue (&action, value))
				{
					bsSetAction (setting, action);
					status = TRUE;
				}
			}
			break;
		case TypeList:
			status = readListValue (setting, keyName);
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

	if (setting->isScreen)
		asprintf (&keyName, "%s:s%d_%s", setting->parent->name, 
				  setting->screenNum, setting->name);
	else
		asprintf (&keyName, "%s:as_%s", setting->parent->name, setting->name);

	if (setting->isDefault)
	{
		iniparser_unset (iniFile, keyName);
		free (keyName);
		return;
	}

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
			{
				BSSettingColorValue value;
				if (bsGetColor (setting, &value))
				{
					char *colString;
					colString = colorToString (&value.array);
					if (!colString)
						break;

					iniparser_setstr (iniFile, keyName, colString);
					free (colString);
				}
			}
			break;
		case TypeAction:
			{
				BSSettingActionValue value;
				if (bsGetAction (setting, &value))
					writeActionValue (&value, keyName);
			}
			break;
		case TypeList:
			writeListValue (setting, keyName);
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
		return NULL;

	for (i = 0; i < nFile; i++)
	{
		pos = strrchr (nameList[i]->d_name, '.');
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

