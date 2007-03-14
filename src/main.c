
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

static void initGeneralOptions(BSContext *context)
{
	/*
	GKeyFile * f;
	gchar * s;
	s = g_strconcat(g_get_home_dir(),"/.beryl/libberylsettings.ini",NULL);
	f = g_key_file_new();
	g_key_file_load_from_file(f,s,0,NULL);
	g_free(s);
	GError *e = NULL;
	context->de_integration =
			g_key_file_get_boolean(f,"general","integration",&e);
	if (e)
		context->de_integration = TRUE;

	s=g_key_file_get_string(f,"general","backend",NULL);
	if (!s)
		beryl_settings_context_set_backend(context,"ini");
	else
	{
		if (!beryl_settings_context_set_backend(context,s))
			beryl_settings_context_set_backend(context,"ini");
		g_free(s);
	}
	s=g_key_file_get_string(f,"general","profile",NULL);
	beryl_settings_context_set_profile(context,s);
	if (s)
		g_free(s);
	g_key_file_free(f);
	*/
	context->deIntegration = FALSE;
	bsSetBackend(context, "ini");
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

	initGeneralOptions(context);
	
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

static void * openBackend(char * backend)
{
	char * home = getenv("HOME");
	void * dlhand = NULL;
	char * dlname = NULL;
	char * err = NULL;
	
	if (home && strlen(home))
	{
		asprintf(&dlname, "%s/.bsettings/backends/lib%s.so",home,backend);
		
		err = dlerror();
		if (err)
			free(err);

		dlhand = dlopen(dlname,RTLD_NOW);
		err = dlerror();
	}
		
	if (err || !dlhand)
	{
		free(dlname);
		asprintf(&dlname, "%s/bsettings/backends/lib%s.so",LIBDIR,backend);
		dlhand = dlopen(dlname,RTLD_NOW);
		err = dlerror();
	}

	free(dlname);
	
	if (err || !dlhand)
	{
		fprintf(stderr, "libbsettings: dlopen: %s\n",err);
		return NULL;
	}

	free(err);

	return dlhand;
}

Bool bsSetBackend(BSContext *context, char *name)
{
	if (context->backend)
	{
		if (context->backend->vTable->backendFini)
			context->backend->vTable->backendFini(context);
		dlclose(context->backend->dlhand);
		free(context->backend);
		context->backend = NULL;
	}
	
	void *dlhand = openBackend(name);
	if (!dlhand)
		dlhand = openBackend("ini");

	if (!dlhand)
		return FALSE;

	BackendGetInfoProc getInfo = dlsym(dlhand,"getBackendInfo");


	if (!getInfo)
	{
		dlclose(dlhand);
		return FALSE;
	}

	BSBackendVTable *vt = getInfo();

	if (vt)
	{
		dlclose(dlhand);
		return FALSE;
	}
	
	context->backend = malloc(sizeof(BSBackend));
	context->backend->dlhand = dlhand;
	context->backend->vTable = vt;

	return TRUE;
}

static void copyValue(BSSettingValue *from, BSSettingValue *to)
{
	memcpy(to, from, sizeof(BSSettingValue));
	BSSettingType type = from->parent->type;

	if (from->isListChild)
		type = from->parent->info.forList.listType;
	
	switch (type)
	{
		case TypeString:
			to->value.asString = strdup(from->value.asString);
			break;
		case TypeMatch:
			to->value.asMatch = strdup(from->value.asMatch);
			break;
		case TypeList:
			to->value.asList = NULL;
			BSSettingValueList *l = from->value.asList;
			while (l)
			{
				NEW(BSSettingValue, value);
				copyValue(l->data, value);
				to->value.asList = bsSettingValueListAppend(to->value.asList,
															value);
				l = l->next;
			}
			break;
		default:
			break;
	}
}

static void copyFromDefault(BSSetting *setting)
{
	if (setting->value != &setting->defaultValue)
		bsFreeSettingValue(setting->value);
	
	NEW(BSSettingValue, value);
	copyValue(&setting->defaultValue, value);
	setting->value = value;
}


Bool bsSetInt(BSSettingValue * value, int data)
{
	if (value->parent->isDefault && (value->parent->defaultValue.value.asInt == data))
		return 1;
	
	value->parent->isDefault = FALSE;
	copyValue(&value->parent->defaultValue, value);
	
	if (!(value->parent->type == TypeInt))
		return 0;
	
	value->value.asInt = data;
	return 1;
} 

Bool bsSetFloat(BSSettingValue * value, float data)
{
	if (value->parent->isDefault && (value->parent->defaultValue.value.asFloat == data))
		return 1;
	value->parent->isDefault = FALSE;
	copyValue(&value->parent->defaultValue, value);

	if (!(value->parent->type == TypeFloat))
		return 0;
	
	value->value.asFloat = data;
	return 1;
}

Bool bsSetBool(BSSettingValue * value, Bool data)
{
	if (value->parent->isDefault && (value->parent->defaultValue.value.asBool == data))
		return 1;
	value->parent->isDefault = FALSE;
	copyValue(&value->parent->defaultValue, value);	

	if (!(value->parent->type == TypeBool))
		return 0;
	
	value->value.asBool = data;
	return 1;
}

Bool bsSetString(BSSettingValue * value, const char * data)
{

	const char * defaultValue = strdup(value->parent->defaultValue.value.asString);
	if (value->parent->isDefault && (strcmp(defaultValue, data)))	
		return 1;
	
	value->parent->isDefault = FALSE;
	copyValue(&value->parent->defaultValue, value);

	if (!(value->parent->type == TypeString))
		return 0;
	
	if (value->value.asString)
		free(value->value.asString);
	value->value.asString = strdup(data);

	return 1;
	
}

Bool bsSetColor(BSSettingValue * value, BSSettingColorValue data)
{
	BSSettingColorValue defaultValue = value->parent->defaultValue.value.asColor;
	if (value->parent->isDefault && (memcmp(&defaultValue, &data, sizeof(BSSettingColorValue))))
		return 1;
	value->parent->isDefault = FALSE;
	copyValue(&value->parent->defaultValue, value);	

	if (!(value->parent->type == TypeColor))
		return 0;
	
	
	value->value.asColor = data;
	return 1;
	
}
Bool bsSetMatch(BSSettingValue * value, const char * data)
{
	const char * defaultValue = strdup(value->parent->defaultValue.value.asMatch);
	if (value->parent->isDefault && (strcmp(defaultValue, data)))	
		return 1;
	value->parent->isDefault = FALSE;

	copyValue(&value->parent->defaultValue, value);


	if (!(value->parent->type == TypeMatch))
		return 0;
	
	if (value->value.asMatch)
		free(value->value.asMatch);
	value->value.asMatch = strdup(data);
	
	return 1;
}
Bool bsSetAction(BSSettingValue * value, BSSettingActionValue data)
{
	BSSettingActionValue defaultValue = value->parent->defaultValue.value.asAction;
	
	if (value->parent->isDefault && (memcmp(&defaultValue, &data, sizeof(BSSettingActionValue))))
		return 1;
	value->parent->isDefault = FALSE;
	copyValue(&value->parent->defaultValue, value);	

	if (!(value->parent->type == TypeAction))
		return 0;
	
	value->value.asAction = data;
	return 1;
}

void bsContextDestroy(BSContext * context)
{
	if (context->backend)
	{
		if (context->backend->vTable->backendFini)
			context->backend->vTable->backendFini(context);
		dlclose(context->backend->dlhand);
		free(context->backend);
		context->backend = NULL;
	}
	bsFreeContext(context);
}
