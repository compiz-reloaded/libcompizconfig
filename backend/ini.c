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
#include "iniparser.h"

#include <X11/X.h>
#include <X11/Xlib.h>

#define DEFAULTPROF "Default"
#define SETTINGPATH ".bsettings/"

typedef struct _IniPrivData
{
	BSContext *context;
	char * lastProfile;
	dictionary * iniFile;
	int iniWatchDesc;
	Bool ignoreEvent;
} IniPrivData;

static IniPrivData *privData = NULL;
static int privDataSize = 0;
static int iniNotifyFd = 0;

static void updateNotify(IniPrivData *data, char *filePath)
{
	if (!iniNotifyFd)
	{
		iniNotifyFd = inotify_init ();
		fcntl (iniNotifyFd, F_SETFL, O_NONBLOCK);
	}

	if (data->iniWatchDesc)
		inotify_rm_watch (iniNotifyFd, data->iniWatchDesc);

	data->iniWatchDesc = inotify_add_watch (iniNotifyFd, filePath, 
											IN_MODIFY | IN_MOVE | IN_CREATE | IN_DELETE);

}
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

static void setProfile(IniPrivData *data, char *profile)
{
	char *fileName;
	struct stat fileStat;

	if (data->iniFile)
		iniparser_freedict (data->iniFile);

	data->iniFile = NULL;

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

	updateNotify (data, fileName);

	/* load the data from the file */
	data->iniFile = iniparser_load (fileName);

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
	bsStringToKeyBinding (token, action);

	token = strsep (&value, ",");
	if (!token)
		goto invalidaction;

	/* button binding */
	bsStringToButtonBinding (token, action);

	token = strsep (&value, ",");
	if (!token)
		goto invalidaction;

	/* edge binding */
	bsStringToEdge (token, action);

	token = strsep (&value, ",");
	if (!token)
		goto invalidaction;

	/* edge button */
	action->edgeButton = strtoul (token, NULL, 10);

	/* bell */
	action->onBell = (strcmp (value, "true") == 0);

	free (valueStart);

	return TRUE;

invalidaction:
	free (valueStart);
	return FALSE;
}

static char * writeActionValue (BSSettingActionValue * action)
{
	char *keyBinding;
	char *buttonBinding;
	char *edge;
	char *actionString = NULL;

	keyBinding = bsKeyBindingToString (action);
	if (!keyBinding)
		keyBinding = strdup("");

	buttonBinding = bsButtonBindingToString (action);
	if (!buttonBinding)
		buttonBinding = strdup("");

	edge = bsEdgeToString (action);
	if (!edge)
		edge = strdup("");

	asprintf (&actionString, "%s,%s,%s,%d,%s\n", keyBinding,
			  buttonBinding, edge, action->edgeButton,
			  action->onBell ? "true" : "false");

	return actionString;
}

static Bool readListValue (IniPrivData *data, BSSetting * setting, char * keyName)
{
	BSSettingValueList list = NULL;
	char *value, *valueStart, *valString;
	char *token;
	int nItems = 0, i = 0;

	valString = iniparser_getstring (data->iniFile, keyName, NULL);
	if (!valString)
		return FALSE;

	value = strdup (valString);
	valueStart = value;

	token = strsep (&value, ";");
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
                   	bsStringToColor(token, &array[i]);
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

static void writeListValue (IniPrivData *data, BSSetting * setting, char * keyName)
{
#define STRINGBUFSIZE 2048
	char stringBuffer[STRINGBUFSIZE]; //FIXME: we should allocate that dynamically
	BSSettingValueList list;

	if (!bsGetList (setting, &list))
		return;

	memset (stringBuffer, 0, sizeof(stringBuffer));

	while (list)
	{
		switch (setting->info.forList.listType)
		{
			case TypeString:
				strncat (stringBuffer, list->data->value.asString, STRINGBUFSIZE);
				break;
			case TypeMatch:
				strncat (stringBuffer, list->data->value.asMatch, STRINGBUFSIZE);
				break;
			case TypeInt:
				{
					char *value = NULL;
					asprintf (&value, "%d", list->data->value.asInt);
					if (!value)
						break;

					strncat (stringBuffer, value, STRINGBUFSIZE);
					free (value);
				}
				break;
			case TypeBool:
				strncat (stringBuffer, (list->data->value.asBool) ? "true" : "false", 
						 STRINGBUFSIZE);
				break;
			case TypeFloat:
				{
					char *value = NULL;
					asprintf (&value, "%f", list->data->value.asFloat);
					if (!value)
						break;

					strncat (stringBuffer, value, STRINGBUFSIZE);
					free (value);
				}
				break;
			case TypeColor:
				{
					char *color = NULL;
					color = bsColorToString(&list->data->value.asColor);
					if (!color)
						break;

					strncat (stringBuffer, color, STRINGBUFSIZE);
					free (color);
				}
				break;
			case TypeAction:
				{
					char *action;
					action = writeActionValue (&list->data->value.asAction);
					if (!action)
						break;
						
					strncat (stringBuffer, action, STRINGBUFSIZE);
					free (action);
				}
			default:
				break;
		}

		strncat (stringBuffer, ";", STRINGBUFSIZE);;
		list = list->next;
	}

	iniparser_setstr (data->iniFile, keyName, stringBuffer);
}

static void processEvents(void)
{
    char	buf[256 * (sizeof (struct inotify_event) + 16)];
	struct  inotify_event *event;
	IniPrivData *data;
	int		len, i = 0, j;

	if (!iniNotifyFd)
		return;

    len = read (iniNotifyFd, buf, sizeof (buf));
	len = -1;

	if (len < 0)
		return;

	while (i < len)
	{
	    event = (struct inotify_event *) &buf[i];
		data = privData;

		for (j = 0, data = privData; j < privDataSize; j++, data++)
			if (!data->ignoreEvent && (data->iniWatchDesc == event->wd))
				break;

		if (j < privDataSize)
		{
			/* our ini file has been modified, reload it */
			char *currentProfile;
			currentProfile = bsGetProfile (data->context);
			if (!currentProfile)
				currentProfile = DEFAULTPROF;

			setProfile (data, currentProfile);
			bsReadSettings (data->context);
		}

	    i += sizeof (*event) + event->len;
    }
}

static Bool initBackend(BSContext * context)
{
	IniPrivData *newData;
	privData = realloc (privData, (privDataSize + 1) * sizeof(IniPrivData));

	newData = privData + privDataSize;

	/* initialize the newly allocated part */
	memset(newData, 0, sizeof(IniPrivData));
	newData->context = context;

	return TRUE;
}

static Bool finiBackend(BSContext * context)
{
	IniPrivData *data;

	data = findPrivFromContext (context);

	if (!data)
		return FALSE;

	if (data->iniFile)
		iniparser_freedict (data->iniFile);

	if (data->iniWatchDesc)
		inotify_rm_watch (iniNotifyFd, data->iniWatchDesc);

	if (data->lastProfile)
		free (data->lastProfile);

	privDataSize--;

	if (privDataSize)
		privData = realloc (privData, privDataSize * sizeof(IniPrivData));
	else
	{
		free (privData);
		if (iniNotifyFd)
			close (iniNotifyFd);
	}

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
	if (!currentProfile)
		currentProfile = DEFAULTPROF;

	if (!data->lastProfile || (strcmp(data->lastProfile, currentProfile) != 0))
		setProfile (data, currentProfile);

	if (data->lastProfile)
		free (data->lastProfile);
	data->lastProfile = NULL;

	if (!data->iniFile)
		return FALSE;

	data->lastProfile = strdup (currentProfile);
	
	return TRUE;
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
		asprintf (&keyName, "%s:s%d_%s", setting->parent->name, 
				  setting->screenNum, setting->name);
	else
		asprintf (&keyName, "%s:as_%s", setting->parent->name, setting->name);

	switch (setting->type)
	{
	    case TypeString:
			{
				char *value;
				value = iniparser_getstring (data->iniFile, keyName, NULL);

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
				value = iniparser_getstring (data->iniFile, keyName, NULL);

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
				value = iniparser_getint (data->iniFile, keyName, 0x7fffffff);

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
				value = iniparser_getboolean (data->iniFile, keyName, 0x7fffffff);

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
				value = (float) iniparser_getdouble (data->iniFile, keyName, 0x7ffffffff);

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
				value = iniparser_getstring (data->iniFile, keyName, NULL);

				if (value && bsStringToColor (value, &color))
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

				value = iniparser_getstring (data->iniFile, keyName, NULL);
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
			status = readListValue (data, setting, keyName);
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
	char *currentProfile;
	IniPrivData *data;

	data = findPrivFromContext (context);
	if (!data)
		return FALSE;

	currentProfile = bsGetProfile (context);
	if (!currentProfile)
		currentProfile = DEFAULTPROF;

	if (!data->lastProfile || (strcmp(data->lastProfile, currentProfile) != 0))
		setProfile (data, currentProfile);

	if (data->lastProfile)
		free (data->lastProfile);
	data->lastProfile = NULL;

	if (!data->iniFile)
		return FALSE;

	data->lastProfile = strdup (currentProfile);

	return TRUE;
}

static void writeSetting(BSContext * context, BSSetting * setting)
{
	char *keyName;
	IniPrivData *data;

	data = findPrivFromContext (context);
	if (!data)
		return;

	if (setting->isScreen)
		asprintf (&keyName, "%s:s%d_%s", setting->parent->name, 
				  setting->screenNum, setting->name);
	else
		asprintf (&keyName, "%s:as_%s", setting->parent->name, setting->name);

	if (setting->isDefault)
	{
		iniparser_unset (data->iniFile, keyName);
		free (keyName);
		return;
	}

	switch (setting->type)
	{
		case TypeString:
			{
				char *value;
				if (bsGetString (setting, &value))
					iniparser_setstr (data->iniFile, keyName, value);
			}
			break;
		case TypeMatch:
			{
				char *value;
				if (bsGetMatch (setting, &value))
					iniparser_setstr (data->iniFile, keyName, value);
			}
			break;
		case TypeInt:
			{
				int value;
				if (bsGetInt (setting, &value))
				{
					char *string = NULL;
					asprintf (&string, "%i", value);
					if (!string)
						break;

					iniparser_setstr (data->iniFile, keyName, string);
					free (string);
				}
			}
			break;
		case TypeFloat:
			{
				float value;
				if (bsGetFloat (setting, &value))
				{
					char *string = NULL;
					asprintf (&string, "%f", value);
					if (!string)
						break;
						
					iniparser_setstr (data->iniFile, keyName, string);
					free (string);
				}
			}
			break;
		case TypeBool:
			{
				Bool value;
				if (bsGetBool (setting, &value))
					iniparser_setstr (data->iniFile, keyName, value ? "true" : "false");
			}
			break;
		case TypeColor:
			{
				BSSettingColorValue value;
				if (bsGetColor (setting, &value))
				{
					char *colString = NULL;
					colString = bsColorToString (&value);
					if (!colString)
						break;

					iniparser_setstr (data->iniFile, keyName, colString);
					free (colString);
				}
			}
			break;
		case TypeAction:
			{
				BSSettingActionValue value;
				if (bsGetAction (setting, &value))
				{
					char *actionString = NULL;
					actionString = writeActionValue (&value);
					if (!actionString)
						break;
						
					iniparser_setstr (data->iniFile, keyName, actionString);
					free (actionString);
				}
			}
			break;
		case TypeList:
			writeListValue (data, setting, keyName);
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
	char *currentProfile;
	IniPrivData *data;

	data = findPrivFromContext (context);
	if (!data)
		return;

	currentProfile = bsGetProfile (context);
	fileName = getIniFileName (currentProfile);

	file = fopen (fileName, "w");

	iniparser_dump_ini (data->iniFile, file);

	/* empty file watch */
	data->ignoreEvent = TRUE;
	processEvents ();
	data->ignoreEvent = FALSE;

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

