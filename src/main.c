#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <dirent.h>
#include <libintl.h>
#include <dlfcn.h>

#include <bsettings.h>

#include "bsettings-private.h"


static int pluginNameFilter (const struct dirent *name)
{
    int length = strlen (name->d_name);

    if (length < 7)
	return 0;

    if (strncmp (name->d_name, "lib", 3) ||
	strncmp (name->d_name + length - 3, ".so", 3))
	return 0;

    return 1;
}

static void loadPlugins(BSContext * context, char * path)
{
	struct dirent **nameList;
    char	  *name;
    int		  nFile, i;

    if (!path)
	    return;
	
	nFile = scandir(path, &nameList, pluginNameFilter, NULL);

	if (!nFile)
		return;

	for (i = 0; i < nFile; i++)
	{
		asprintf(&name, "%s/%s", path, nameList[i]->d_name);
		free(nameList[i]);
		
		bsLoadPlugin(context, name);
		free(name);
	}
	free(nameList);
	
}

BSContext * bsContextNew(void)
{

	NEW(BSContext,context);
	
	char * home = getenv("HOME");

	if (home && strlen(home))
	{
		char *homeplugins = NULL;
		asprintf(&homeplugins, "%s/.compiz/plugins",home);
		loadPlugins(context,homeplugins);
		free(homeplugins);
	}
	loadPlugins(context,PLUGINDIR);
	
	return context;
}

BSPlugin * bsFindPlugin(BSContext *context, char * name)
{
	BSPluginList *l = context->plugins;
	while (l)
	{
		if (!strcmp(l->data->name, name))
			return l->data;
		l = l->next;
	}
	return NULL;
}

BSSetting * bsFindSetting(BSPlugin *plugin, char * name,
						  Bool isScreen, unsigned int screenNum)
{
	BSSettingList *l = plugin->settings;
	while (l)
	{
		if (!strcmp(l->data->name, name) && l->data->isScreen == isScreen &&
		    l->data->screenNum == screenNum)
			return l->data;
		l = l->next;
	}
	return NULL;
}
