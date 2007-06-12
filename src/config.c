/*
 * Compiz configuration system library
 * 
 * Copyright (C) 2007  Dennis Kasprzyk <onestone@opencompositing.org>
 * Copyright (C) 2007  Danny Baumann <maniac@opencompositing.org>
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
 
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "ccs-private.h"

static char *getConfigFileName(void)
{
	char *home;
	char *fileName = NULL;
	
	home = getenv("HOME");

	if (!home || !strlen(home))
		return NULL;

	asprintf(&fileName, "%s/.compizconfig/config",home);

	return fileName;
}

static IniDictionary *getConfigFile(void)
{
	char *fileName;
	IniDictionary *iniFile;

	fileName = getConfigFileName();
	if (!fileName)
		return NULL;

	iniFile = ccsIniOpen (fileName);

	free (fileName);
	return iniFile;
}

unsigned int ccsAddConfigWatch(CCSContext *context, FileWatchCallbackProc callback)
{
    unsigned int ret;
    char *fileName;

    fileName = getConfigFileName();
    if (!fileName)
	return 0;

    ret = ccsAddFileWatch (fileName, TRUE, callback, context);
    free (fileName);

    return ret;
}

Bool ccsReadConfig(ConfigOption option, char** value)
{
	IniDictionary *iniFile;
	char *entry = NULL;
	Bool ret;

	iniFile = getConfigFile();
	if (!iniFile)
		return FALSE;

	switch (option)
	{
		case OptionProfile:
			entry = "profile";
			break;
		case OptionBackend:
			entry = "backend";
			break;
		case OptionIntegration:
			entry = "integration";
			break;
		default:
			break;
	}

	if (!entry)
	{
		ccsIniClose (iniFile);
		return FALSE;
	}

	*value = NULL;

	ret = ccsIniGetString (iniFile, "general", entry, value);

	ccsIniClose (iniFile);
	return ret;
}

Bool ccsWriteConfig(ConfigOption option, char* value)
{
	IniDictionary *iniFile;
	char *entry = NULL;
	char *fileName;

	iniFile = getConfigFile();
	if (!iniFile)
		return FALSE;

	switch (option)
	{
		case OptionProfile:
			entry = "profile";
			break;
		case OptionBackend:
			entry = "backend";
			break;
		case OptionIntegration:
			entry = "integration";
			break;
		default:
			break;
	}

	if (!entry)
	{
		ccsIniClose (iniFile);
		return FALSE;
	}

	ccsIniSetString (iniFile, "general", entry, value);

	fileName = getConfigFileName();
	if (!fileName)
	{
		ccsIniClose (iniFile);
		return FALSE;
	}
	ccsIniSave (iniFile, fileName);

	ccsIniClose (iniFile);
	free (fileName);
	return TRUE;
}

