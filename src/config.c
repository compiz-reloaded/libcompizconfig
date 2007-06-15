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

static char *getSectionName(void)
{
	char *profile;
	char *section;
	
	profile = getenv("COMPIZ_CONFIG_PROFILE");

	if (profile && strlen(profile))
	{
		asprintf(&section, "general_%s",profile);
		return section;
	}

	profile = getenv("GNOME_DESKTOP_SESSION_ID");
	if (profile && strlen(profile))
		return strdup("gnome_session");

	profile = getenv("KDE_FULL_SESSION");
	if (profile && strlen(profile) && strcasecmp(profile,"true") == 0)
		return strdup("kde_session");

	return strdup("general");
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

static Bool ccsReadGlobalConfig(ConfigOption option, char** value)
{
	IniDictionary *iniFile;
	char *entry = NULL;
	char *section;
	Bool ret;
	FILE *fp;

    iniFile = ccsIniOpen (SYSCONFDIR "/compizconfig/config");
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

	section = getSectionName();
	
	ret = ccsIniGetString (iniFile, section, entry, value);

	free(section);
	ccsIniClose (iniFile);
	return ret;
}

Bool ccsReadConfig(ConfigOption option, char** value)
{
	IniDictionary *iniFile;
	char *entry = NULL;
	char *section;
	Bool ret;

	iniFile = getConfigFile();
	if (!iniFile)
		return ccsReadGlobalConfig(option, value);

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

	section = getSectionName();

	ret = ccsIniGetString (iniFile, section, entry, value);

	free(section);
	ccsIniClose (iniFile);

	if (!ret)
		ret = ccsReadGlobalConfig(option, value);
	return ret;
}

Bool ccsWriteConfig(ConfigOption option, char* value)
{
	IniDictionary *iniFile;
	char *entry = NULL;
	char *section;
	char *fileName;
	char *curVal;

	/* don't change config if nothing changed */
	if (ccsReadConfig(option, &curVal) && strcmp(value, curVal) == 0)
		return TRUE;

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

	section = getSectionName();

	ccsIniSetString (iniFile, section, entry, value);

	free(section);
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

