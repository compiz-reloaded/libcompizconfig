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

static void subGroupAdd(BSSetting * setting, BSGroup * group)
{
	BSSubGroupList *l = group->subGroups;
	while (l)
	{
		if (!strcmp(l->data->name, setting->subGroup))
		{
			l->data->settings = bsSettingListAppend(l->data->settings,
													 setting);
			return;
		}
		l = l->next;
	}
	
	NEW(BSSubGroup, subGroup);
	group->subGroups = bsSubGroupListAppend(group->subGroups, subGroup);
	subGroup->name = strdup(setting->subGroup);
	subGroup->settings = bsSettingListAppend(subGroup->settings, setting);
}

static void groupAdd(BSSetting * setting, BSPlugin * plugin)
{
	BSGroupList *l = plugin->groups;
	while (l)
	{
		if (!strcmp(l->data->name, setting->group))
		{
			subGroupAdd(setting, l->data);
			return;
		}
		l = l->next;
	}
	
	NEW(BSGroup, group);
	plugin->groups = bsGroupListAppend(plugin->groups, group);
	group->name = strdup(setting->group);
	subGroupAdd(setting, group);
}

void collateGroups(BSPlugin * plugin)
{
	BSSettingList *l = plugin->settings;
	while (l)
	{
		groupAdd(l->data, plugin);
		l = l->next;
	}	
}

void bsFreeContext(BSContext *c)
{
	if (!c)
		return;
	bsPluginListFree(c->plugins, TRUE);
	free(c);
}

void bsFreePlugin(BSPlugin *p)
{
	if (!p)
		return;
	free(p->name);
	free(p->shortDesc);
	free(p->longDesc);
	free(p->hints);
	free(p->category);
	free(p->filename);
	bsStringListFree(p->loadAfter, TRUE);
	bsStringListFree(p->loadBefore, TRUE);
	bsStringListFree(p->provides, TRUE);
	bsStringListFree(p->requires, TRUE);
	bsSettingListFree(p->settings, TRUE);
	bsGroupListFree(p->groups, TRUE);
	free(p);
}

void bsFreeSetting(BSSetting *s)
{
	if (!s)
		return;
	free(s->name);
	free(s->shortDesc);
	free(s->longDesc);
	free(s->group);
	free(s->subGroup);
	free(s->hints);

	switch (s->type)
	{
		case TypeString:
			bsStringListFree(s->info.forString.allowedValues, TRUE);
			break;
		case TypeList:
			if (s->info.forList.listType == TypeString)
				bsStringListFree(s->info.forList.listInfo->
								 forString.allowedValues, TRUE);
			free(s->info.forList.listInfo);
			break;
		default:
			break;
	}
	
	if (&s->defaultValue != s->value)
		bsFreeSettingValue(s->value);
	bsFreeSettingValue(&s->defaultValue);
	free (s);
}

void bsFreeGroup(BSGroup *g)
{
	if (!g)
		return;
	free(g->name);
	bsSubGroupListFree(g->subGroups, TRUE);
	free(g);
}
void bsFreeSubGroup(BSSubGroup *s)
{
	if (!s)
		return;
	free(s->name);
	bsSettingListFree(s->settings, FALSE);
	free(s);
}

void bsFreeSettingValue(BSSettingValue *v)
{
	if (!v)
		return;
	if (!v->parent)
		return;

	BSSettingType type = v->parent->type;

	if (v->isListChild)
		type = v->parent->info.forList.listType;
	
	switch (type)
	{
		case TypeString:
			free(v->value.asString);
			break;
		case TypeMatch:
			free(v->value.asMatch);
			break;
		case TypeList:
			if (!v->isListChild)
				bsSettingValueListFree(v->value.asList, TRUE);
			break;
		default:
			break;
	}
	
	
	if (v != &v->parent->defaultValue)
		free(v);
}
