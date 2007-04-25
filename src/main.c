
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <libintl.h>
#include <dlfcn.h>

#include <bsettings.h>

#include "bsettings-private.h"

static void
initGeneralOptions (BSContext * context)
{
	char *val;
	if (bsReadConfig (OptionBackend, &val))
	{
		bsSetBackend (context, val);
		free (val);
	}
	else
	{
		bsSetBackend (context, "ini");
	}
	if (bsReadConfig (OptionProfile, &val))
	{
		bsSetProfile (context, val);
		free (val);
	}
	else
	{
		bsSetProfile (context, "");
	}
	if (bsReadConfig (OptionIntegration, &val))
	{
		bsSetIntegrationEnabled (context, !strcasecmp (val, "true"));
		free (val);
	}
	else
	{
		bsSetIntegrationEnabled (context, TRUE);
	}
}

BSContext *
bsContextNew (void)
{

	NEW (BSContext, context);

	bsLoadPlugins (context);

	initGeneralOptions (context);

	return context;
}

BSPlugin *
bsFindPlugin (BSContext * context, char *name)
{
	if (!name)
		name = "";
	BSPluginList l = context->plugins;
	while (l)
	{
		if (!strcmp (l->data->name, name))
			return l->data;
		l = l->next;
	}
	return NULL;
}

BSSetting *
bsFindSetting (BSPlugin * plugin, char *name,
			   Bool isScreen, unsigned int screenNum)
{
	BSSettingList l = plugin->settings;
	while (l)
	{
		if (!strcmp (l->data->name, name) && l->data->isScreen == isScreen &&
			l->data->screenNum == screenNum)
			return l->data;
		l = l->next;
	}
	return NULL;
}

Bool
bsPluginIsActive (BSContext * context, char *name)
{
	BSPlugin *plugin;
	BSSetting *setting;
	Bool ret;

	plugin = bsFindPlugin (context, name);
	if (!plugin)
		return FALSE;

	setting = bsFindSetting (plugin, "____plugin_enabled", FALSE, 0);
	if (!setting)
		return FALSE;

	bsGetBool (setting, &ret);

	return ret;
}


static void
subGroupAdd (BSSetting * setting, BSGroup * group)
{
	BSSubGroupList l = group->subGroups;
	while (l)
	{
		if (!strcmp (l->data->name, setting->subGroup))
		{
			l->data->settings = bsSettingListAppend (l->data->settings,
													 setting);
			return;
		}
		l = l->next;
	}

	NEW (BSSubGroup, subGroup);
	group->subGroups = bsSubGroupListAppend (group->subGroups, subGroup);
	subGroup->name = strdup (setting->subGroup);
	subGroup->settings = bsSettingListAppend (subGroup->settings, setting);
}

static void
groupAdd (BSSetting * setting, BSPlugin * plugin)
{
	BSGroupList l = plugin->groups;
	while (l)
	{
		if (!strcmp (l->data->name, setting->group))
		{
			subGroupAdd (setting, l->data);
			return;
		}
		l = l->next;
	}

	NEW (BSGroup, group);
	plugin->groups = bsGroupListAppend (plugin->groups, group);
	group->name = strdup (setting->group);
	subGroupAdd (setting, group);
}

void
collateGroups (BSPlugin * plugin)
{
	BSSettingList l = plugin->settings;
	while (l)
	{
		groupAdd (l->data, plugin);
		l = l->next;
	}
}

void
bsFreeContext (BSContext * c)
{
	if (!c)
		return;
	if (c->profile)
		free (c->profile);

	if (c->changedSettings)
		bsSettingListFree (c->changedSettings, FALSE);
	bsPluginListFree (c->plugins, TRUE);
	free (c);
}

void
bsFreePlugin (BSPlugin * p)
{
	if (!p)
		return;
	free (p->name);
	free (p->shortDesc);
	free (p->longDesc);
	free (p->hints);
	free (p->category);
	free (p->filename);
	bsStringListFree (p->loadAfter, TRUE);
	bsStringListFree (p->loadBefore, TRUE);
	bsStringListFree (p->requiresPlugin, TRUE);
	bsStringListFree (p->providesFeature, TRUE);
	bsStringListFree (p->requiresFeature, TRUE);
	bsSettingListFree (p->settings, TRUE);
	bsGroupListFree (p->groups, TRUE);
	free (p);
}

void
bsFreeSetting (BSSetting * s)
{
	if (!s)
		return;
	free (s->name);
	free (s->shortDesc);
	free (s->longDesc);
	free (s->group);
	free (s->subGroup);
	free (s->hints);

	switch (s->type)
	{
	case TypeString:
		bsStringListFree (s->info.forString.allowedValues, TRUE);
		break;
	case TypeList:
		if (s->info.forList.listType == TypeString)
			bsStringListFree (s->info.forList.listInfo->
							  forString.allowedValues, TRUE);
		free (s->info.forList.listInfo);
		break;
	default:
		break;
	}

	if (&s->defaultValue != s->value)
		bsFreeSettingValue (s->value);
	bsFreeSettingValue (&s->defaultValue);
	free (s);
}

void
bsFreeGroup (BSGroup * g)
{
	if (!g)
		return;
	free (g->name);
	bsSubGroupListFree (g->subGroups, TRUE);
	free (g);
}

void
bsFreeSubGroup (BSSubGroup * s)
{
	if (!s)
		return;
	free (s->name);
	bsSettingListFree (s->settings, FALSE);
	free (s);
}

void
bsFreeSettingValue (BSSettingValue * v)
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
		free (v->value.asString);
		break;
	case TypeMatch:
		free (v->value.asMatch);
		break;
	case TypeList:
		if (!v->isListChild)
			bsSettingValueListFree (v->value.asList, TRUE);
		break;
	default:
		break;
	}


	if (v != &v->parent->defaultValue)
		free (v);
}

void
bsFreePluginConflict (BSPluginConflict * c)
{
	if (!c)
		return;
	free (c->value);
	if (c->plugins)
		c->plugins = bsPluginListFree (c->plugins, FALSE);
}

void
bsFreeActionConflict (BSActionConflict * c)
{
	if (!c)
		return;
	if (c->settings)
		c->settings = bsSettingListFree (c->settings, FALSE);
}

static void *
openBackend (char *backend)
{
	char *home = getenv ("HOME");
	void *dlhand = NULL;
	char *dlname = NULL;
	char *err = NULL;

	if (home && strlen (home))
	{
		asprintf (&dlname, "%s/.bsettings/backends/lib%s.so", home, backend);

		err = dlerror ();
		if (err)
			free (err);

		dlhand = dlopen (dlname, RTLD_NOW);
		err = dlerror ();
	}

	if (err || !dlhand)
	{
		free (dlname);
		asprintf (&dlname, "%s/bsettings/backends/lib%s.so", LIBDIR, backend);
		dlhand = dlopen (dlname, RTLD_NOW);
		err = dlerror ();
	}

	free (dlname);

	if (err || !dlhand)
	{
		fprintf (stderr, "libbsettings: dlopen: %s\n", err);
		return NULL;
	}

	free (err);

	return dlhand;
}

Bool
bsSetBackend (BSContext * context, char *name)
{
	if (context->backend)
	{
		if (context->backend->vTable->backendFini)
			context->backend->vTable->backendFini (context);
		dlclose (context->backend->dlhand);
		free (context->backend);
		context->backend = NULL;
	}

	void *dlhand = openBackend (name);
	if (!dlhand)
	{
		name = "ini";
		dlhand = openBackend (name);
	}

	if (!dlhand)
		return FALSE;

	BackendGetInfoProc getInfo = dlsym (dlhand, "getBackendInfo");


	if (!getInfo)
	{
		dlclose (dlhand);
		return FALSE;
	}

	BSBackendVTable *vt = getInfo ();

	if (!vt)
	{
		dlclose (dlhand);
		return FALSE;
	}

	context->backend = malloc (sizeof (BSBackend));
	context->backend->dlhand = dlhand;
	context->backend->vTable = vt;
	if (context->backend->vTable->backendInit)
		context->backend->vTable->backendInit (context);

	bsWriteConfig (OptionBackend, name);
	return TRUE;
}

static void
copyValue (BSSettingValue * from, BSSettingValue * to)
{
	memcpy (to, from, sizeof (BSSettingValue));
	BSSettingType type = from->parent->type;

	if (from->isListChild)
		type = from->parent->info.forList.listType;

	switch (type)
	{
	case TypeString:
		to->value.asString = strdup (from->value.asString);
		break;
	case TypeMatch:
		to->value.asMatch = strdup (from->value.asMatch);
		break;
	case TypeList:
		to->value.asList = NULL;
		BSSettingValueList l = from->value.asList;
		while (l)
		{
			NEW (BSSettingValue, value);
			copyValue (l->data, value);
			to->value.asList = bsSettingValueListAppend (to->value.asList,
														 value);
			l = l->next;
		}
		break;
	default:
		break;
	}
}

static void
copyFromDefault (BSSetting * setting)
{
	if (setting->value != &setting->defaultValue)
		bsFreeSettingValue (setting->value);

	NEW (BSSettingValue, value);
	copyValue (&setting->defaultValue, value);
	setting->value = value;
	setting->isDefault = FALSE;
}

void
bsResetToDefault (BSSetting * setting)
{
	if (setting->value != &setting->defaultValue)
	{
		bsFreeSettingValue (setting->value);
		if (!strcmp (setting->name, "____plugin_enabled"))
			setting->parent->context->pluginsChanged = TRUE;
		else
			setting->parent->context->changedSettings =
				bsSettingListAppend (setting->parent->context->
									 changedSettings, setting);
	}
	setting->value = &setting->defaultValue;
	setting->isDefault = TRUE;

}

Bool
bsSetInt (BSSetting * setting, int data)
{
	if (setting->type != TypeInt)
		return FALSE;

	if (setting->isDefault && (setting->defaultValue.value.asInt == data))
		return TRUE;

	if (!setting->isDefault && (setting->defaultValue.value.asInt == data))
	{
		bsResetToDefault (setting);
		return TRUE;
	}

	if (setting->value->value.asInt == data)
	    return TRUE;

	if ((data < setting->info.forInt.min) ||
		(data > setting->info.forInt.max))
		return FALSE;

	if (setting->isDefault)
		copyFromDefault (setting);

	setting->value->value.asInt = data;
	setting->parent->context->changedSettings =
		bsSettingListAppend (setting->parent->context->changedSettings,
							 setting);
	return TRUE;
}

Bool
bsSetFloat (BSSetting * setting, float data)
{
	if (setting->type != TypeFloat)
		return FALSE;

	if (setting->isDefault && (setting->defaultValue.value.asFloat == data))
		return TRUE;

	if (!setting->isDefault && (setting->defaultValue.value.asFloat == data))
	{
		bsResetToDefault (setting);
		return TRUE;
	}

	if (setting->value->value.asFloat == data)
	    return TRUE;

	if ((data < setting->info.forFloat.min) ||
		(data > setting->info.forFloat.max))
		return FALSE;

	if (setting->isDefault)
		copyFromDefault (setting);

	setting->value->value.asFloat = data;
	setting->parent->context->changedSettings =
		bsSettingListAppend (setting->parent->context->changedSettings,
							 setting);
	return TRUE;
}

Bool
bsSetBool (BSSetting * setting, Bool data)
{
	if (setting->type != TypeBool)
		return FALSE;

	if (setting->isDefault && (setting->defaultValue.value.asBool == data))
		return TRUE;

	if (!setting->isDefault && (setting->defaultValue.value.asBool == data))
	{
		bsResetToDefault (setting);
		return TRUE;
	}

	if (setting->value->value.asBool == data)
	    return TRUE;

	if (setting->isDefault)
		copyFromDefault (setting);

	setting->value->value.asBool = data;

	if (!strcmp (setting->name, "____plugin_enabled"))
		setting->parent->context->pluginsChanged = TRUE;
	else
		setting->parent->context->changedSettings =
			bsSettingListAppend (setting->parent->context->changedSettings,
								 setting);
	return TRUE;
}

Bool
bsSetString (BSSetting * setting, const char *data)
{
	if (setting->type != TypeString)
		return FALSE;

	if (!data)
		return FALSE;

	Bool isDefault = strcmp (setting->defaultValue.value.asString, data) == 0;

	if (setting->isDefault && isDefault)
		return TRUE;

	if (!setting->isDefault && isDefault)
	{
		bsResetToDefault (setting);
		return TRUE;
	}

	if (!strcmp(setting->value->value.asString,data))
	    return TRUE;

	BSStringList allowed = setting->info.forString.allowedValues;
	if (allowed)
	{
		while (allowed)
		{
			if (strcmp (allowed->data, data) == 0)
				break;
			allowed = allowed->next;
		}

		if (!allowed)
			/* if allowed is NULL here, it means that none of the
			   allowed values matched the string to set */
			return FALSE;
	}

	if (setting->isDefault)
		copyFromDefault (setting);

	free (setting->value->value.asString);
	setting->value->value.asString = strdup (data);
	setting->parent->context->changedSettings =
		bsSettingListAppend (setting->parent->context->changedSettings,
							 setting);
	return TRUE;
}

Bool
bsSetColor (BSSetting * setting, BSSettingColorValue data)
{
	if (setting->type != TypeColor)
		return FALSE;

	BSSettingColorValue defValue = setting->defaultValue.value.asColor;
	Bool isDefault = bsIsEqualColor (defValue, data);

	if (setting->isDefault && isDefault)
		return TRUE;

	if (!setting->isDefault && isDefault)
	{
		bsResetToDefault (setting);
		return TRUE;
	}

	if (bsIsEqualColor(setting->value->value.asColor,data))
	    return TRUE;

	if (setting->isDefault)
		copyFromDefault (setting);

	setting->value->value.asColor = data;
	setting->parent->context->changedSettings =
		bsSettingListAppend (setting->parent->context->changedSettings,
							 setting);
	return TRUE;
}

Bool
bsSetMatch (BSSetting * setting, const char *data)
{
	if (setting->type != TypeMatch)
		return FALSE;

	if (!data)
		return FALSE;

	Bool isDefault = strcmp (setting->defaultValue.value.asMatch, data) == 0;

	if (setting->isDefault && isDefault)
		return TRUE;

	if (!setting->isDefault && isDefault)
	{
		bsResetToDefault (setting);
		return TRUE;
	}

	if (!strcmp(setting->value->value.asMatch,data))
	    return TRUE;

	if (setting->isDefault)
		copyFromDefault (setting);

	free (setting->value->value.asMatch);
	setting->value->value.asMatch = strdup (data);
	setting->parent->context->changedSettings =
		bsSettingListAppend (setting->parent->context->changedSettings,
							 setting);
	return TRUE;
}

Bool
bsSetAction (BSSetting * setting, BSSettingActionValue data)
{
	if (setting->type != TypeAction)
		return FALSE;

	BSSettingActionValue defValue = setting->defaultValue.value.asAction;
	Bool isDefault = bsIsEqualAction (data, defValue);

	if (setting->isDefault && isDefault)
		return TRUE;

	if (!setting->isDefault && isDefault)
	{
		bsResetToDefault (setting);
		return TRUE;
	}

	if (bsIsEqualAction(setting->value->value.asAction,data))
	    return TRUE;

	if (setting->isDefault)
		copyFromDefault (setting);

	if (setting->info.forAction.key)
	{
		setting->value->value.asAction.keysym = data.keysym;
		setting->value->value.asAction.keyModMask = data.keyModMask;
	}
	if (setting->info.forAction.button)
	{
		setting->value->value.asAction.button = data.button;
		setting->value->value.asAction.buttonModMask = data.buttonModMask;
	}
	if (setting->info.forAction.edge)
	{
		setting->value->value.asAction.edgeButton = data.edgeButton;
		setting->value->value.asAction.edgeMask = data.edgeMask;
	}
	if (setting->info.forAction.bell)
		setting->value->value.asAction.onBell = data.onBell;

	setting->parent->context->changedSettings =
		bsSettingListAppend (setting->parent->context->changedSettings,
							 setting);

	return TRUE;
}

static Bool
bsCompareLists (BSSettingValueList l1, BSSettingValueList l2,
				BSSettingListInfo info)
{
	while (l1 && l2)
	{
		switch (info.listType)
		{
		case TypeInt:
			if (l1->data->value.asInt != l2->data->value.asInt)
				return FALSE;
			break;
		case TypeBool:
			if (l1->data->value.asBool != l2->data->value.asBool)
				return FALSE;
			break;
		case TypeFloat:
			if (l1->data->value.asFloat != l2->data->value.asFloat)
				return FALSE;
			break;
		case TypeString:
			if (strcmp (l1->data->value.asString, l2->data->value.asString))
				return FALSE;
			break;
		case TypeMatch:
			if (strcmp (l1->data->value.asMatch, l2->data->value.asMatch))
				return FALSE;
			break;
		case TypeAction:
			if (!bsIsEqualAction
				(l1->data->value.asAction, l2->data->value.asAction))
				return FALSE;
			break;
		case TypeColor:
			if (!bsIsEqualColor
				(l1->data->value.asColor, l2->data->value.asColor))
				return FALSE;
			break;
		default:
			return FALSE;
			break;
		}
		l1 = l1->next;
		l2 = l2->next;
	}
	if ((!l1 && l2) || (l1 && !l2))
		return FALSE;
	return TRUE;
}

static BSSettingValueList
bsCopyList (BSSettingValueList l1, BSSetting * setting)
{
	BSSettingValueList l2 = NULL;
	while (l1)
	{
		NEW (BSSettingValue, value);
		value->parent = setting;
		value->isListChild = TRUE;
		switch (setting->info.forList.listType)
		{
		case TypeInt:
			value->value.asInt = l1->data->value.asInt;
			break;
		case TypeBool:
			value->value.asBool = l1->data->value.asBool;
			break;
		case TypeFloat:
			value->value.asFloat = l1->data->value.asFloat;
			break;
		case TypeString:
			value->value.asString = strdup (l1->data->value.asString);
			break;
		case TypeMatch:
			value->value.asMatch = strdup (l1->data->value.asMatch);
			break;
		case TypeAction:
			memcpy (&value->value.asAction, &l1->data->value.asAction,
					sizeof (BSSettingActionValue));
			break;
		case TypeColor:
			memcpy (&value->value.asColor, &l1->data->value.asColor,
					sizeof (BSSettingColorValue));
			break;
		default:
			return FALSE;
			break;
		}
		l2 = bsSettingValueListAppend (l2, value);
		l1 = l1->next;
	}

	return l2;
}

Bool
bsSetList (BSSetting * setting, BSSettingValueList data)
{
	if (setting->type != TypeList)
		return FALSE;

	Bool isDefault = bsCompareLists (setting->defaultValue.value.asList, data,
									 setting->info.forList);

	if (setting->isDefault && isDefault)
		return TRUE;

	if (!setting->isDefault && isDefault)
	{
		bsResetToDefault (setting);
		return TRUE;
	}

	if (bsCompareLists (setting->value->value.asList, data,
									 setting->info.forList))
	    return TRUE;
	
	if (setting->isDefault)
		copyFromDefault (setting);

	bsSettingValueListFree (setting->value->value.asList, TRUE);
	setting->value->value.asList = bsCopyList (data, setting);

	setting->parent->context->changedSettings =
		bsSettingListAppend (setting->parent->context->changedSettings,
							 setting);

	return TRUE;
}

Bool
bsSetValue (BSSetting * setting, BSSettingValue * data)
{
	switch (setting->type)
	{
	case TypeInt:
		return bsSetInt (setting, data->value.asInt);
		break;
	case TypeFloat:
		return bsSetFloat (setting, data->value.asFloat);
		break;
	case TypeBool:
		return bsSetBool (setting, data->value.asBool);
		break;
	case TypeColor:
		return bsSetColor (setting, data->value.asColor);
		break;
	case TypeString:
		return bsSetString (setting, data->value.asString);
		break;
	case TypeMatch:
		return bsSetMatch (setting, data->value.asMatch);
		break;
	case TypeAction:
		return bsSetAction (setting, data->value.asAction);
		break;
	case TypeList:
		return bsSetList (setting, data->value.asList);
	default:
		break;
	}
	return FALSE;
}

Bool
bsGetInt (BSSetting * setting, int *data)
{
	if (setting->type != TypeInt)
		return FALSE;

	*data = setting->value->value.asInt;
	return TRUE;
}

Bool
bsGetFloat (BSSetting * setting, float *data)
{
	if (setting->type != TypeFloat)
		return FALSE;

	*data = setting->value->value.asFloat;
	return TRUE;
}

Bool
bsGetBool (BSSetting * setting, Bool * data)
{
	if (setting->type != TypeBool)
		return FALSE;

	*data = setting->value->value.asBool;
	return TRUE;
}

Bool
bsGetString (BSSetting * setting, char **data)
{
	if (setting->type != TypeString)
		return FALSE;

	*data = setting->value->value.asString;
	return TRUE;
}

Bool
bsGetColor (BSSetting * setting, BSSettingColorValue * data)
{
	if (setting->type != TypeColor)
		return TRUE;

	*data = setting->value->value.asColor;
	return TRUE;
}

Bool
bsGetMatch (BSSetting * setting, char **data)
{
	if (setting->type != TypeMatch)
		return FALSE;

	*data = setting->value->value.asMatch;
	return TRUE;
}

Bool
bsGetAction (BSSetting * setting, BSSettingActionValue * data)
{
	if (setting->type != TypeAction)
		return FALSE;

	*data = setting->value->value.asAction;
	return TRUE;
}

Bool
bsGetList (BSSetting * setting, BSSettingValueList * data)
{
	if (setting->type != TypeList)
		return FALSE;

	*data = setting->value->value.asList;
	return TRUE;
}

void
bsContextDestroy (BSContext * context)
{
	if (context->backend)
	{
		if (context->backend->vTable->backendFini)
			context->backend->vTable->backendFini (context);
		dlclose (context->backend->dlhand);
		free (context->backend);
		context->backend = NULL;
	}
	bsFreeContext (context);
}

BSPluginList
bsGetActivePluginList (BSContext * context)
{
	BSPluginList rv = NULL;
	BSPluginList l = context->plugins;
	Bool active;
	while (l)
	{
		BSSetting *setting = bsFindSetting (l->data, "____plugin_enabled",
											FALSE, 0);
		if (setting && bsGetBool (setting, &active) && active &&
			strcmp (l->data->name, "bset"))
			rv = bsPluginListAppend (rv, l->data);
		l = l->next;
	}
	return rv;
}

static BSPlugin *
findPluginInList (BSPluginList list, char *name)
{
	if (!name || !strlen (name))
		return NULL;

	while (list)
	{
		if (!strcmp (list->data->name, name))
			return list->data;
		list = list->next;
	}
	return NULL;
}

typedef struct _PluginSortHelper
{
	BSPlugin *plugin;
	BSPluginList after;
} PluginSortHelper;

BSStringList
bsGetSortedPluginStringList (BSContext * context)
{
	BSPluginList ap = bsGetActivePluginList (context);
	BSPluginList list = ap;
	BSPlugin *p = NULL;
	BSStringList rv = NULL;
	PluginSortHelper *ph = NULL;

	int len = bsPluginListLength (ap);
	int i, j;
	// TODO: conflict handling

	PluginSortHelper *plugins = malloc (len * sizeof (PluginSortHelper));
	for (i = 0; i < len; i++, list = list->next)
	{
		plugins[i].plugin = list->data;
		plugins[i].after = NULL;
	}

	for (i = 0; i < len; i++)
	{
		BSStringList l = plugins[i].plugin->loadAfter;
		while (l)
		{
			p = findPluginInList (ap, l->data);
			if (p)
				plugins[i].after = bsPluginListAppend (plugins[i].after, p);
			l = l->next;
		}

		l = plugins[i].plugin->loadBefore;
		while (l)
		{
			p = findPluginInList (ap, l->data);
			if (p)
			{
				ph = NULL;
				for (j = 0; j < len; j++)
					if (p == plugins[j].plugin)
						ph = &plugins[j];
				if (ph)
					ph->after = bsPluginListAppend (ph->after,
													plugins[i].plugin);
			}
			l = l->next;
		}
	}

	bsPluginListFree (ap, FALSE);

	Bool error = FALSE;
	int removed = 0;
	Bool found;

	while (!error && removed < len)
	{
		found = FALSE;
		for (i = 0; i < len; i++)
		{
			if (!plugins[i].plugin)
				continue;
			if (plugins[i].after)
				continue;
			found = TRUE;
			removed++;
			p = plugins[i].plugin;
			plugins[i].plugin = NULL;


			for (j = 0; j < len; j++)
				plugins[j].after =
					bsPluginListRemove (plugins[j].after, p, FALSE);

			rv = bsStringListAppend (rv, strdup (p->name));
		}
		if (!found)
			error = TRUE;
	}

	if (error)
	{
		fprintf (stderr,
				 "libbsettings: unable to generate sorted plugin list\n");
		for (i = 0; i < len; i++)
		{
			bsPluginListFree (plugins[i].after, FALSE);
		}
		bsStringListFree (rv, TRUE);
		rv = NULL;
	}
	free (plugins);
	return rv;
}

Bool
bsGetIntegrationEnabled (BSContext * context)
{
	if (!context)
		return FALSE;
	return context->deIntegration;
}

char *
bsGetProfile (BSContext * context)
{
	if (!context)
		return NULL;
	return context->profile;
}

void
bsSetIntegrationEnabled (BSContext * context, Bool value)
{
	context->deIntegration = value;
	bsWriteConfig (OptionIntegration, (value) ? "true" : "false");
}

void
bsSetProfile (BSContext * context, char *name)
{
	if (context->profile)
		free (context->profile);
	context->profile = (name) ? strdup (name) : strdup ("");
	bsWriteConfig (OptionProfile, context->profile);
}

void
bsProcessEvents (BSContext * context)
{
	if (!context)
		return;

	bsCheckFileWatches ();

	if (context->backend && context->backend->vTable->executeEvents)
		(*context->backend->vTable->executeEvents) ();
}

void
bsReadSettings (BSContext * context)
{
	if (!context || !context->backend)
		return;

	if (!context->backend->vTable->readSetting)
		return;

	if (context->backend->vTable->readInit)
		if (!(*context->backend->vTable->readInit) (context))
			return;

	BSPluginList pl = context->plugins;
	while (pl)
	{
		BSSettingList sl = pl->data->settings;
		while (sl)
		{
			(*context->backend->vTable->readSetting) (context, sl->data);
			sl = sl->next;
		}
		pl = pl->next;
	}
	if (context->backend->vTable->readDone)
		(*context->backend->vTable->readDone) (context);
}

void
bsWriteSettings (BSContext * context)
{
	if (!context || !context->backend)
		return;
	if (!context->backend->vTable->writeSetting)
		return;

	if (context->backend->vTable->writeInit)
		if (!(*context->backend->vTable->writeInit) (context))
			return;
	BSPluginList pl = context->plugins;
	while (pl)
	{
		BSSettingList sl = pl->data->settings;
		while (sl)
		{
			(*context->backend->vTable->writeSetting) (context, sl->data);
			sl = sl->next;
		}
		pl = pl->next;
	}
	if (context->backend->vTable->writeDone)
		(*context->backend->vTable->writeDone) (context);
	context->pluginsChanged = FALSE;
	context->changedSettings =
		bsSettingListFree (context->changedSettings, FALSE);
}

void
bsWriteChangedSettings (BSContext * context)
{
	if (!context || !context->backend)
		return;
	if (!context->backend->vTable->writeSetting)
		return;

	if (context->backend->vTable->writeInit)
		if (!(*context->backend->vTable->writeInit) (context))
			return;
	if (context->pluginsChanged)
	{
		BSPluginList pl = context->plugins;
		while (pl)
		{
			BSSetting *s =
				bsFindSetting (pl->data, "____plugin_enabled", FALSE, 0);
			if (s)
				(*context->backend->vTable->writeSetting) (context, s);
			pl = pl->next;
		}
	}
	if (bsSettingListLength (context->changedSettings))
	{
		BSSettingList l = context->changedSettings;
		while (l)
		{
			(*context->backend->vTable->writeSetting) (context, l->data);
			l = l->next;
		}
	}
	if (context->backend->vTable->writeDone)
		(*context->backend->vTable->writeDone) (context);
	context->pluginsChanged = FALSE;
	context->changedSettings =
		bsSettingListFree (context->changedSettings, FALSE);
}

#define FIELDCOMPARABLE(field1, field2) \
	{ \
		typeof(field1) _field2=field2; \
		if (field1 != _field2) \
			return 0;\
	}
#define FOURFIELDS(field1, field2, field3, field4) \
	FIELDCOMPARABLE(field1, field2) \
	FIELDCOMPARABLE(field3, field4)
#define EIGHTFIELDS(field1, field2, field3, field4, field5, field6, field7, field8) \
	FOURFIELDS(field1, field2, field3, field4) \
	FOURFIELDS(field5, field6, field7, field8)

Bool
bsIsEqualColor (BSSettingColorValue c1, BSSettingColorValue c2)
{
	EIGHTFIELDS (c1.color.red, c2.color.red, c1.color.blue, c2.color.blue,
				 c1.color.green, c2.color.green, c1.color.alpha,
				 c2.color.alpha) return 1;
}

Bool
bsIsEqualAction (BSSettingActionValue c1, BSSettingActionValue c2)
{
	EIGHTFIELDS (c1.button, c2.button, c1.buttonModMask, c2.buttonModMask,
				 c1.keysym, c2.keysym, c1.keyModMask,
				 c2.keyModMask) FOURFIELDS (c1.onBell, c2.onBell, c1.edgeMask,
											c2.edgeMask) return 1;
}


Bool
bsPluginSetActive (BSPlugin * plugin, Bool value)
{
	if (!plugin)
		return FALSE;
	BSSetting *s = bsFindSetting (plugin, "____plugin_enabled", FALSE, 0);
	if (!s)
		return FALSE;
	bsSetBool (s, value);

	return TRUE;
}

BSPluginConflictList
bsCanEnablePlugin (BSContext * context, BSPlugin * plugin)
{
	BSPluginConflictList list = NULL;
	BSPluginList pl, pl2;
	BSStringList sl;

	/* look if the plugin to be loaded requires a plugin not present */
	sl = plugin->requiresPlugin;
	while (sl)
	{
		if (!bsFindPlugin (context, sl->data))
		{
			NEW (BSPluginConflict, conflict);
			conflict->value = strdup (sl->data);
			conflict->type = ConflictPluginError;
			conflict->plugins = NULL;
			list = bsPluginConflictListAppend (list, conflict);
		}
		else if (!bsPluginIsActive (context, sl->data))
		{
			/* we've not seen a matching plugin */
			NEW (BSPluginConflict, conflict);
			conflict->value = strdup (sl->data);
			conflict->type = ConflictRequiresPlugin;
			conflict->plugins =
				bsPluginListAppend (conflict->plugins,
									bsFindPlugin (context, sl->data));
			list = bsPluginConflictListAppend (list, conflict);
		}
		sl = sl->next;
	}

	/* look if the new plugin wants a non-present feature */
	sl = plugin->requiresFeature;
	while (sl)
	{
		pl = context->plugins;
		pl2 = NULL;
		while (pl)
		{
			BSStringList featureList = pl->data->providesFeature;

			while (featureList)
			{
				if (strcmp (sl->data, featureList->data) == 0)
				{
					pl2 = bsPluginListAppend (pl2, pl->data);
					break;
				}
				featureList = featureList->next;
			}

			pl = pl->next;
		}

		pl = pl2;

		while (pl)
		{
			if (bsPluginIsActive (context, pl->data->name))
			{
				bsPluginListFree (pl2, FALSE);
				break;
			}
			pl = pl->next;
		}

		if (!pl)
		{
			/* no plugin provides that feature */
			NEW (BSPluginConflict, conflict);

			conflict->value = strdup (sl->data);
			conflict->type = ConflictRequiresFeature;
			conflict->plugins = pl2;

			list = bsPluginConflictListAppend (list, conflict);
		}

		sl = sl->next;
	}

	/* look if another plugin provides the same feature */
	sl = plugin->providesFeature;
	while (sl)
	{
		pl = context->plugins;
		BSPluginConflict *conflict = NULL;
		while (pl)
		{
			if (bsPluginIsActive (context, pl->data->name))
			{
				BSStringList featureList = pl->data->providesFeature;
				while (featureList)
				{
					if (strcmp (sl->data, featureList->data) == 0)
					{
						if (!conflict)
						{
							conflict = calloc (1, sizeof (BSPluginConflict));
							conflict->value = strdup (sl->data);
							conflict->type = ConflictSameFeature;
						}

						conflict->plugins =
							bsPluginListAppend (conflict->plugins, pl->data);
					}
					featureList = featureList->next;
				}
			}
			pl = pl->next;
		}

		if (conflict)
			list = bsPluginConflictListAppend (list, conflict);

		sl = sl->next;
	}

	return list;
}

BSPluginConflictList
bsCanDisablePlugin (BSContext * context, BSPlugin * plugin)
{
	BSPluginConflictList list = NULL;
	BSPluginConflict *conflict = NULL;
	BSPluginList pl;
	BSStringList sl;

	/* look if the plugin to be unloaded is required by another plugin */
	pl = context->plugins;
	for (; pl; pl = pl->next)
	{
		BSStringList pluginList;

		if (pl->data == plugin)
			continue;

		if (!bsPluginIsActive (context, pl->data->name))
			continue;

		pluginList = pl->data->requiresPlugin;
		while (pluginList)
		{
			if (strcmp (plugin->name, pluginList->data) == 0)
			{
				if (!conflict)
				{
					conflict = calloc (1, sizeof (BSPluginConflict));
					conflict->value = strdup (plugin->name);
					conflict->type = ConflictPluginNeeded;
				}
				conflict->plugins =
					bsPluginListAppend (conflict->plugins, pl->data);

				break;
			}

			pluginList = pluginList->next;
		}
	}

	if (conflict)
	{
		list = bsPluginConflictListAppend (list, conflict);
		conflict = NULL;
	}

	/* look if a feature provided is required by another plugin */
	sl = plugin->providesFeature;
	while (sl)
	{
		pl = context->plugins;

		for (; pl; pl = pl->next)
		{
			BSStringList pluginList;

			if (pl->data == plugin)
				continue;

			if (!bsPluginIsActive (context, pl->data->name))
				continue;

			pluginList = pl->data->requiresPlugin;
			while (pluginList)
			{
				if (strcmp (sl->data, pluginList->data) == 0)
				{
					if (!conflict)
					{
						conflict = calloc (1, sizeof (BSPluginConflict));

						conflict->value = strdup (sl->data);
						conflict->type = ConflictPluginNeeded;
					}

					conflict->plugins =
						bsPluginListAppend (conflict->plugins, pl->data);
				}

				pluginList = pluginList->next;
			}

			if (conflict)
				list = bsPluginConflictListAppend (list, conflict);
		}

		sl = sl->next;
	}

	return list;
}

static Bool
conflictKey (BSSettingActionValue a1, BSSettingActionValue a2)
{
	if (a1.keysym == a2.keysym && a1.keyModMask == a2.keyModMask)
		return TRUE;
	return FALSE;
}

static Bool
conflictButton (BSSettingActionValue a1, BSSettingActionValue a2)
{
	if (a1.button == a2.button && a1.buttonModMask == a2.buttonModMask)
		return TRUE;
	return FALSE;
}

static Bool
conflictEdge (BSSettingActionValue a1, BSSettingActionValue a2)
{
	if (a1.edgeMask & a2.edgeMask)
		return TRUE;
	return FALSE;
}


BSActionConflictList
bsCanSetAction (BSContext * context, BSSettingActionValue action)
{
	BSActionConflictList rv = NULL;
	NEW (BSActionConflict, keyC);
	NEW (BSActionConflict, buttonC);
	NEW (BSActionConflict, edgeC);

	keyC->type = ConflictKey;
	buttonC->type = ConflictButton;
	edgeC->type = ConflictEdge;

	BSPluginList pl = context->plugins;
	BSSettingList sl = NULL;
	BSSetting *s;

	while (pl)
	{
		sl = pl->data->settings;
		while (sl)
		{
			s = sl->data;
			if (s->type == TypeAction)
			{
				if (conflictKey (action, s->value->value.asAction))
					keyC->settings = bsSettingListAppend (keyC->settings, s);
				if (conflictButton (action, s->value->value.asAction))
					buttonC->settings =
						bsSettingListAppend (buttonC->settings, s);
				if (conflictEdge (action, s->value->value.asAction))
					edgeC->settings =
						bsSettingListAppend (edgeC->settings, s);
			}
			else if (s->type == TypeList
					 && s->info.forList.listType == TypeAction)
			{
				BSSettingValueList vl = s->value->value.asList;
				while (vl)
				{
					if (conflictKey (action, vl->data->value.asAction))
						keyC->settings =
							bsSettingListAppend (keyC->settings, s);
					if (conflictButton (action, vl->data->value.asAction))
						buttonC->settings =
							bsSettingListAppend (buttonC->settings, s);
					if (conflictEdge (action, vl->data->value.asAction))
						edgeC->settings =
							bsSettingListAppend (edgeC->settings, s);
					vl = vl->next;
				}
			}
			sl = sl->next;
		}
		pl = pl->next;
	}

	if (keyC->settings)
	{
		rv = bsActionConflictListAppend (rv, keyC);
	}
	else
	{
		free (keyC);
	}

	if (buttonC->settings)
	{
		rv = bsActionConflictListAppend (rv, buttonC);
	}
	else
	{
		free (buttonC);
	}

	if (edgeC->settings)
	{
		rv = bsActionConflictListAppend (rv, edgeC);
	}
	else
	{
		free (edgeC);
	}

	return rv;
}
