#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "bsettings-private.h"

static char *getConfigFileName(void)
{
	char *home;
	char *fileName = NULL;
	
	home = getenv("HOME");

	if (!home || !strlen(home))
		return NULL;

	asprintf(&fileName, "%s/.bsettings/config",home);

	return fileName;
}

static IniDictionary *getConfigFile(void)
{
	char *fileName;
	IniDictionary *iniFile;

	fileName = getConfigFileName();
	if (!fileName)
		return NULL;

	iniFile = bsIniOpen (fileName);

	free (fileName);
	return iniFile;
}

unsigned int bsAddConfigWatch(BSContext *context, FileWatchCallbackProc callback)
{
    unsigned int ret;
    char *fileName;

    fileName = getConfigFileName();
    if (!fileName)
	return 0;

    ret = bsAddFileWatch (fileName, TRUE, callback, context);
    free (fileName);

    return ret;
}

Bool bsReadConfig(ConfigOption option, char** value)
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

	if (!option)
	{
		bsIniClose (iniFile);
		return FALSE;
	}

	*value = NULL;

	ret = bsIniGetString (iniFile, "general", entry, value);

	bsIniClose (iniFile);
	return ret;
}

Bool bsWriteConfig(ConfigOption option, char* value)
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
		bsIniClose (iniFile);
		return FALSE;
	}

	bsIniSetString (iniFile, "general", entry, value);

	fileName = getConfigFileName();
	if (!fileName)
	{
		bsIniClose (iniFile);
		return FALSE;
	}
	bsIniSave (iniFile, fileName);

	bsIniClose (iniFile);
	free (fileName);
	return TRUE;
}

