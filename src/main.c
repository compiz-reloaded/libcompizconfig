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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <libintl.h>
#include <dlfcn.h>
#include <dirent.h>
#include <math.h>

#include <ccs.h>

#include "ccs-private.h"

static void
initGeneralOptions (CCSContext * context)
{
	char *val;
	if (ccsReadConfig (OptionBackend, &val))
	{
		ccsSetBackend (context, val);
		free (val);
	}
	else
	{
		ccsSetBackend (context, "ini");
	}
	if (ccsReadConfig (OptionProfile, &val))
	{
		ccsSetProfile (context, val);
		free (val);
	}
	else
	{
		ccsSetProfile (context, "");
	}
	if (ccsReadConfig (OptionIntegration, &val))
	{
		ccsSetIntegrationEnabled (context, !strcasecmp (val, "true"));
		free (val);
	}
	else
	{
		ccsSetIntegrationEnabled (context, TRUE);
	}
}

static void
configChangeNotify (unsigned int watchId, void *closure)
{
	CCSContext *context = (CCSContext *) closure;

	initGeneralOptions (context);
	ccsReadSettings (context);
}

CCSContext *
ccsContextNew (unsigned int *screens, unsigned int numScreens)
{

	NEW (CCSContext, context);

	if (numScreens > 0 && screens)
	{
		int i;
		
		context->screens = calloc(1, sizeof(unsigned int) * numScreens);
		context->numScreens = numScreens;
		for (i = 0; i < numScreens; i++)
			context->screens[i] = screens[i];
	}
	else
	{
		context->screens = calloc(1, sizeof(unsigned int));
		context->screens[0] = 0;
		context->numScreens = 1;
	}
	
	ccsLoadPlugins (context);

	initGeneralOptions (context);
	context->configWatchId = ccsAddConfigWatch (context, configChangeNotify);

	return context;
}

CCSPlugin *
ccsFindPlugin (CCSContext * context, char *name)
{
	if (!name)
		name = "";
	CCSPluginList l = context->plugins;
	while (l)
	{
		if (!strcmp (l->data->name, name))
			return l->data;
		l = l->next;
	}
	return NULL;
}

CCSSetting *
ccsFindSetting (CCSPlugin * plugin, char *name,
			   Bool isScreen, unsigned int screenNum)
{
	PLUGIN_PRIV(plugin);
	if (!pPrivate->loaded && strcmp(name,"____plugin_enabled"))
		ccsLoadPluginSettings (plugin);
	
	CCSSettingList l = pPrivate->settings;
	while (l)
	{
		if (!strcmp (l->data->name, name) && 
		    ((!l->data->isScreen && !isScreen) || (l->data->isScreen && isScreen)) &&
		    (!isScreen || (l->data->screenNum == screenNum)))
			return l->data;
		l = l->next;
	}
	return NULL;
}

Bool
ccsPluginIsActive (CCSContext * context, char *name)
{
	CCSPlugin *plugin;
	CCSSetting *setting;
	Bool ret;

	plugin = ccsFindPlugin (context, name);
	if (!plugin)
		return FALSE;

	setting = ccsFindSetting (plugin, "____plugin_enabled", FALSE, 0);
	if (!setting)
		return FALSE;

	ccsGetBool (setting, &ret);

	return ret;
}


static void
subGroupAdd (CCSSetting * setting, CCSGroup * group)
{
	CCSSubGroupList l = group->subGroups;
	while (l)
	{
		if (!strcmp (l->data->name, setting->subGroup))
		{
			l->data->settings = ccsSettingListAppend (l->data->settings,
													 setting);
			return;
		}
		l = l->next;
	}

	NEW (CCSSubGroup, subGroup);
	group->subGroups = ccsSubGroupListAppend (group->subGroups, subGroup);
	subGroup->name = strdup (setting->subGroup);
	subGroup->settings = ccsSettingListAppend (subGroup->settings, setting);
}

static void
groupAdd (CCSSetting * setting, CCSPluginPrivate * p)
{
	CCSGroupList l = p->groups;
	while (l)
	{
		if (!strcmp (l->data->name, setting->group))
		{
			subGroupAdd (setting, l->data);
			return;
		}
		l = l->next;
	}

	NEW (CCSGroup, group);
	p->groups = ccsGroupListAppend (p->groups, group);
	group->name = strdup (setting->group);
	subGroupAdd (setting, group);
}

void
collateGroups (CCSPluginPrivate * p)
{
	CCSSettingList l = p->settings;
	while (l)
	{
		groupAdd (l->data, p);
		l = l->next;
	}
}

void
ccsFreeContext (CCSContext * c)
{
	if (!c)
		return;

	if (c->profile)
		free (c->profile);

	if (c->configWatchId)
		ccsRemoveFileWatch (c->configWatchId);

	if (c->changedSettings)
		ccsSettingListFree (c->changedSettings, FALSE);

	if (c->screens)
		free (c->screens);

	ccsPluginListFree (c->plugins, TRUE);
	free (c);
}

void
ccsFreePlugin (CCSPlugin * p)
{
	if (!p)
		return;
	free (p->name);
	free (p->shortDesc);
	free (p->longDesc);
	free (p->hints);
	free (p->category);
	ccsStringListFree (p->loadAfter, TRUE);
	ccsStringListFree (p->loadBefore, TRUE);
	ccsStringListFree (p->requiresPlugin, TRUE);
	ccsStringListFree (p->conflictPlugin, TRUE);
	ccsStringListFree (p->conflictFeature, TRUE);
	ccsStringListFree (p->providesFeature, TRUE);
	ccsStringListFree (p->requiresFeature, TRUE);

	PLUGIN_PRIV(p);
	
	ccsSettingListFree (pPrivate->settings, TRUE);
	ccsGroupListFree (pPrivate->groups, TRUE);
	if (pPrivate->xmlFile)
		free (pPrivate->xmlFile);
	if (pPrivate->xmlPath)
		free (pPrivate->xmlPath);
	free (pPrivate);
	free (p);
}

void
ccsFreeSetting (CCSSetting * s)
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
	case TypeInt:
		ccsIntDescListFree(s->info.forInt.desc, TRUE);
		break;
	case TypeList:
		if (s->info.forList.listType == TypeInt)
			ccsIntDescListFree (s->info.forList.listInfo->
							  forInt.desc, TRUE);
		free (s->info.forList.listInfo);
		break;
	default:
		break;
	}

	if (&s->defaultValue != s->value)
		ccsFreeSettingValue (s->value);
	ccsFreeSettingValue (&s->defaultValue);
	free (s);
}

void
ccsFreeGroup (CCSGroup * g)
{
	if (!g)
		return;
	free (g->name);
	ccsSubGroupListFree (g->subGroups, TRUE);
	free (g);
}

void
ccsFreeSubGroup (CCSSubGroup * s)
{
	if (!s)
		return;
	free (s->name);
	ccsSettingListFree (s->settings, FALSE);
	free (s);
}

void
ccsFreeSettingValue (CCSSettingValue * v)
{
	if (!v)
		return;
	if (!v->parent)
		return;

	CCSSettingType type = v->parent->type;

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
			ccsSettingValueListFree (v->value.asList, TRUE);
		break;
	default:
		break;
	}


	if (v != &v->parent->defaultValue)
		free (v);
}

void
ccsFreePluginConflict (CCSPluginConflict * c)
{
	if (!c)
		return;
	free (c->value);
	if (c->plugins)
		c->plugins = ccsPluginListFree (c->plugins, FALSE);
}

void
ccsFreeActionConflict (CCSActionConflict * c)
{
	if (!c)
		return;
	if (c->settings)
		c->settings = ccsSettingListFree (c->settings, FALSE);
}

void
ccsFreeBackendInfo (CCSBackendInfo * b)
{
	if (!b)
		return;
	if (b->name)
		free (b->name);
	if (b->shortDesc)
		free (b->shortDesc);
	if (b->longDesc)
		free (b->longDesc);
}

void
ccsFreeIntDesc (CCSIntDesc * i)
{
	if (!i)
		return;
	if (i->name)
		free (i->name);
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
		asprintf (&dlname, "%s/.compizconfig/backends/lib%s.so", home, backend);

		dlerror ();
		dlhand = dlopen (dlname, RTLD_NOW);
		err = dlerror ();
	}

	if (err || !dlhand)
	{
		free (dlname);
		asprintf (&dlname, "%s/compizconfig/backends/lib%s.so", LIBDIR, backend);
		dlhand = dlopen (dlname, RTLD_NOW);
		err = dlerror ();
	}

	free (dlname);

	if (err || !dlhand)
	{
		fprintf (stderr, "libccs: dlopen: %s\n", err);
		return NULL;
	}

	return dlhand;
}

Bool
ccsSetBackend (CCSContext * context, char *name)
{
	if (context->backend)
	{
		/* no action needed if the backend is the same */
		if (strcmp (context->backend->vTable->name, name) == 0)
			return TRUE;

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

	CCSBackendVTable *vt = getInfo ();

	if (!vt)
	{
		dlclose (dlhand);
		return FALSE;
	}

	context->backend = malloc (sizeof (CCSBackend));
	context->backend->dlhand = dlhand;
	context->backend->vTable = vt;
	if (context->backend->vTable->backendInit)
		context->backend->vTable->backendInit (context);

	ccsDisableFileWatch (context->configWatchId);
	ccsWriteConfig (OptionBackend, name);
	ccsEnableFileWatch (context->configWatchId);

	return TRUE;
}

static void
copyValue (CCSSettingValue * from, CCSSettingValue * to)
{
	memcpy (to, from, sizeof (CCSSettingValue));
	CCSSettingType type = from->parent->type;

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
		CCSSettingValueList l = from->value.asList;
		while (l)
		{
			NEW (CCSSettingValue, value);
			copyValue (l->data, value);
			to->value.asList = ccsSettingValueListAppend (to->value.asList,
														 value);
			l = l->next;
		}
		break;
	default:
		break;
	}
}

static void
copyFromDefault (CCSSetting * setting)
{
	if (setting->value != &setting->defaultValue)
		ccsFreeSettingValue (setting->value);

	NEW (CCSSettingValue, value);
	copyValue (&setting->defaultValue, value);
	setting->value = value;
	setting->isDefault = FALSE;
}

void
ccsResetToDefault (CCSSetting * setting)
{
	if (setting->value != &setting->defaultValue)
	{
		ccsFreeSettingValue (setting->value);
		if (!strcmp (setting->name, "____plugin_enabled"))
			setting->parent->context->pluginsChanged = TRUE;
		else
			setting->parent->context->changedSettings =
				ccsSettingListAppend (setting->parent->context->
									 changedSettings, setting);
	}
	setting->value = &setting->defaultValue;
	setting->isDefault = TRUE;

}

Bool
ccsSetInt (CCSSetting * setting, int data)
{
	if (setting->type != TypeInt)
		return FALSE;

	if (setting->isDefault && (setting->defaultValue.value.asInt == data))
		return TRUE;

	if (!setting->isDefault && (setting->defaultValue.value.asInt == data))
	{
		ccsResetToDefault (setting);
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
		ccsSettingListAppend (setting->parent->context->changedSettings,
							 setting);
	return TRUE;
}

Bool
ccsSetFloat (CCSSetting * setting, float data)
{
	if (setting->type != TypeFloat)
		return FALSE;

	if (setting->isDefault && (setting->defaultValue.value.asFloat == data))
		return TRUE;

	if (!setting->isDefault && (setting->defaultValue.value.asFloat == data))
	{
		ccsResetToDefault (setting);
		return TRUE;
	}

	/* allow the values to differ a tiny bit because of 
	   possible rounding / precision issues */
	if (fabs (setting->value->value.asFloat - data) < 1e-5)
		return TRUE;

	if ((data < setting->info.forFloat.min) ||
		(data > setting->info.forFloat.max))
		return FALSE;

	if (setting->isDefault)
		copyFromDefault (setting);

	setting->value->value.asFloat = data;
	setting->parent->context->changedSettings =
		ccsSettingListAppend (setting->parent->context->changedSettings,
							 setting);
	return TRUE;
}

Bool
ccsSetBool (CCSSetting * setting, Bool data)
{
	if (setting->type != TypeBool)
		return FALSE;

	if (setting->isDefault
		&& ((setting->defaultValue.value.asBool && data)
			|| (!setting->defaultValue.value.asBool && !data)))
		return TRUE;

	if (!setting->isDefault
		&& ((setting->defaultValue.value.asBool && data)
			|| (!setting->defaultValue.value.asBool && !data)))
	{
		ccsResetToDefault (setting);
		return TRUE;
	}

	if ((setting->value->value.asBool && data)
		|| (!setting->value->value.asBool && !data))
		return TRUE;

	if (setting->isDefault)
		copyFromDefault (setting);

	setting->value->value.asBool = data;

	if (!strcmp (setting->name, "____plugin_enabled"))
		setting->parent->context->pluginsChanged = TRUE;
	else
		setting->parent->context->changedSettings =
			ccsSettingListAppend (setting->parent->context->changedSettings,
								 setting);
	return TRUE;
}

Bool
ccsSetString (CCSSetting * setting, const char *data)
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
		ccsResetToDefault (setting);
		return TRUE;
	}

	if (!strcmp (setting->value->value.asString, data))
		return TRUE;

	if (setting->isDefault)
		copyFromDefault (setting);

	free (setting->value->value.asString);
	setting->value->value.asString = strdup (data);
	setting->parent->context->changedSettings =
		ccsSettingListAppend (setting->parent->context->changedSettings,
							 setting);
	return TRUE;
}

Bool
ccsSetColor (CCSSetting * setting, CCSSettingColorValue data)
{
	if (setting->type != TypeColor)
		return FALSE;

	CCSSettingColorValue defValue = setting->defaultValue.value.asColor;
	Bool isDefault = ccsIsEqualColor (defValue, data);

	if (setting->isDefault && isDefault)
		return TRUE;

	if (!setting->isDefault && isDefault)
	{
		ccsResetToDefault (setting);
		return TRUE;
	}

	if (ccsIsEqualColor (setting->value->value.asColor, data))
		return TRUE;

	if (setting->isDefault)
		copyFromDefault (setting);

	setting->value->value.asColor = data;
	setting->parent->context->changedSettings =
		ccsSettingListAppend (setting->parent->context->changedSettings,
							 setting);
	return TRUE;
}

Bool
ccsSetMatch (CCSSetting * setting, const char *data)
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
		ccsResetToDefault (setting);
		return TRUE;
	}

	if (!strcmp (setting->value->value.asMatch, data))
		return TRUE;

	if (setting->isDefault)
		copyFromDefault (setting);

	free (setting->value->value.asMatch);
	setting->value->value.asMatch = strdup (data);
	setting->parent->context->changedSettings =
		ccsSettingListAppend (setting->parent->context->changedSettings,
							 setting);
	return TRUE;
}

Bool
ccsSetAction (CCSSetting * setting, CCSSettingActionValue data)
{
	if (setting->type != TypeAction)
		return FALSE;

	CCSSettingActionValue defValue = setting->defaultValue.value.asAction;
	Bool isDefault = ccsIsEqualAction (data, defValue);

	if (setting->isDefault && isDefault)
		return TRUE;

	if (!setting->isDefault && isDefault)
	{
		ccsResetToDefault (setting);
		return TRUE;
	}

	if (ccsIsEqualAction (setting->value->value.asAction, data))
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
		ccsSettingListAppend (setting->parent->context->changedSettings,
							 setting);

	return TRUE;
}

static Bool
ccsCompareLists (CCSSettingValueList l1, CCSSettingValueList l2,
				CCSSettingListInfo info)
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
			if (!ccsIsEqualAction
				(l1->data->value.asAction, l2->data->value.asAction))
				return FALSE;
			break;
		case TypeColor:
			if (!ccsIsEqualColor
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

static CCSSettingValueList
ccsCopyList (CCSSettingValueList l1, CCSSetting * setting)
{
	CCSSettingValueList l2 = NULL;
	while (l1)
	{
		NEW (CCSSettingValue, value);
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
					sizeof (CCSSettingActionValue));
			break;
		case TypeColor:
			memcpy (&value->value.asColor, &l1->data->value.asColor,
					sizeof (CCSSettingColorValue));
			break;
		default:
			return FALSE;
			break;
		}
		l2 = ccsSettingValueListAppend (l2, value);
		l1 = l1->next;
	}

	return l2;
}

Bool
ccsSetList (CCSSetting * setting, CCSSettingValueList data)
{
	if (setting->type != TypeList)
		return FALSE;

	Bool isDefault = ccsCompareLists (setting->defaultValue.value.asList, data,
									 setting->info.forList);

	if (setting->isDefault && isDefault)
		return TRUE;

	if (!setting->isDefault && isDefault)
	{
		ccsResetToDefault (setting);
		return TRUE;
	}

	if (ccsCompareLists (setting->value->value.asList, data,
						setting->info.forList))
		return TRUE;

	if (setting->isDefault)
		copyFromDefault (setting);

	ccsSettingValueListFree (setting->value->value.asList, TRUE);
	setting->value->value.asList = ccsCopyList (data, setting);

	setting->parent->context->changedSettings =
		ccsSettingListAppend (setting->parent->context->changedSettings,
							 setting);

	return TRUE;
}

Bool
ccsSetValue (CCSSetting * setting, CCSSettingValue * data)
{
	switch (setting->type)
	{
	case TypeInt:
		return ccsSetInt (setting, data->value.asInt);
		break;
	case TypeFloat:
		return ccsSetFloat (setting, data->value.asFloat);
		break;
	case TypeBool:
		return ccsSetBool (setting, data->value.asBool);
		break;
	case TypeColor:
		return ccsSetColor (setting, data->value.asColor);
		break;
	case TypeString:
		return ccsSetString (setting, data->value.asString);
		break;
	case TypeMatch:
		return ccsSetMatch (setting, data->value.asMatch);
		break;
	case TypeAction:
		return ccsSetAction (setting, data->value.asAction);
		break;
	case TypeList:
		return ccsSetList (setting, data->value.asList);
	default:
		break;
	}
	return FALSE;
}

Bool
ccsGetInt (CCSSetting * setting, int *data)
{
	if (setting->type != TypeInt)
		return FALSE;

	*data = setting->value->value.asInt;
	return TRUE;
}

Bool
ccsGetFloat (CCSSetting * setting, float *data)
{
	if (setting->type != TypeFloat)
		return FALSE;

	*data = setting->value->value.asFloat;
	return TRUE;
}

Bool
ccsGetBool (CCSSetting * setting, Bool * data)
{
	if (setting->type != TypeBool)
		return FALSE;

	*data = setting->value->value.asBool;
	return TRUE;
}

Bool
ccsGetString (CCSSetting * setting, char **data)
{
	if (setting->type != TypeString)
		return FALSE;

	*data = setting->value->value.asString;
	return TRUE;
}

Bool
ccsGetColor (CCSSetting * setting, CCSSettingColorValue * data)
{
	if (setting->type != TypeColor)
		return TRUE;

	*data = setting->value->value.asColor;
	return TRUE;
}

Bool
ccsGetMatch (CCSSetting * setting, char **data)
{
	if (setting->type != TypeMatch)
		return FALSE;

	*data = setting->value->value.asMatch;
	return TRUE;
}

Bool
ccsGetAction (CCSSetting * setting, CCSSettingActionValue * data)
{
	if (setting->type != TypeAction)
		return FALSE;

	*data = setting->value->value.asAction;
	return TRUE;
}

Bool
ccsGetList (CCSSetting * setting, CCSSettingValueList * data)
{
	if (setting->type != TypeList)
		return FALSE;

	*data = setting->value->value.asList;
	return TRUE;
}

void
ccsContextDestroy (CCSContext * context)
{
	if (!context)
	    return;
	if (context->backend)
	{
		if (context->backend->vTable->backendFini)
			context->backend->vTable->backendFini (context);
		dlclose (context->backend->dlhand);
		free (context->backend);
		context->backend = NULL;
	}
	ccsFreeContext (context);
}

CCSPluginList
ccsGetActivePluginList (CCSContext * context)
{
	CCSPluginList rv = NULL;
	CCSPluginList l = context->plugins;
	Bool active;
	while (l)
	{
		CCSSetting *setting = ccsFindSetting (l->data, "____plugin_enabled",
											FALSE, 0);
		if (setting && ccsGetBool (setting, &active) && active &&
			strcmp (l->data->name, "ccp"))
			rv = ccsPluginListAppend (rv, l->data);
		l = l->next;
	}
	return rv;
}

static CCSPlugin *
findPluginInList (CCSPluginList list, char *name)
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
	CCSPlugin *plugin;
	CCSPluginList after;
} PluginSortHelper;

CCSStringList
ccsGetSortedPluginStringList (CCSContext * context)
{
	CCSPluginList ap = ccsGetActivePluginList (context);
	CCSPluginList list = ap;
	CCSPlugin *p = NULL;
	CCSStringList rv = NULL;
	PluginSortHelper *ph = NULL;

	int len = ccsPluginListLength (ap);
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
		CCSStringList l = plugins[i].plugin->loadAfter;
		while (l)
		{
			p = findPluginInList (ap, l->data);
			if (p && !ccsPluginListFind(plugins[i].after, p))
				plugins[i].after = ccsPluginListAppend (plugins[i].after, p);
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
				if (ph && !ccsPluginListFind(ph->after, plugins[i].plugin))
					ph->after = ccsPluginListAppend (ph->after,
													plugins[i].plugin);
			}
			l = l->next;
		}
	}

	ccsPluginListFree (ap, FALSE);

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
					ccsPluginListRemove (plugins[j].after, p, FALSE);

			rv = ccsStringListAppend (rv, strdup (p->name));
		}
		if (!found)
			error = TRUE;
	}

	if (error)
	{
		fprintf (stderr,
				 "libccs: unable to generate sorted plugin list\n");
		for (i = 0; i < len; i++)
		{
			ccsPluginListFree (plugins[i].after, FALSE);
		}
		ccsStringListFree (rv, TRUE);
		rv = NULL;
	}
	free (plugins);
	return rv;
}

Bool
ccsGetIntegrationEnabled (CCSContext * context)
{
	if (!context)
		return FALSE;
	return context->deIntegration;
}

char *
ccsGetProfile (CCSContext * context)
{
	if (!context)
		return NULL;
	return context->profile;
}

void
ccsSetIntegrationEnabled (CCSContext * context, Bool value)
{
	context->deIntegration = value;

	ccsDisableFileWatch (context->configWatchId);
	ccsWriteConfig (OptionIntegration, (value) ? "true" : "false");
	ccsEnableFileWatch (context->configWatchId);
}

void
ccsSetProfile (CCSContext * context, char *name)
{
	if (context->profile)
		free (context->profile);
	context->profile = (name) ? strdup (name) : strdup ("");

	ccsDisableFileWatch (context->configWatchId);
	ccsWriteConfig (OptionProfile, context->profile);
	ccsEnableFileWatch (context->configWatchId);
}

void
ccsProcessEvents (CCSContext * context, unsigned int flags)
{
	if (!context)
		return;

	ccsCheckFileWatches ();

	if (context->backend && context->backend->vTable->executeEvents)
		(*context->backend->vTable->executeEvents) (flags);
}

void
ccsReadSettings (CCSContext * context)
{
	if (!context || !context->backend)
		return;

	if (!context->backend->vTable->readSetting)
		return;

	if (context->backend->vTable->readInit)
		if (!(*context->backend->vTable->readInit) (context))
			return;

	CCSPluginList pl = context->plugins;
	while (pl)
	{
		PLUGIN_PRIV(pl->data);
		CCSSettingList sl = pPrivate->settings;
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
ccsReadPluginSettings (CCSPlugin * plugin)
{
	if (!plugin)
		return;
	CCSContext *context = plugin->context;
	
	if (!context || !context->backend)
		return;

	if (!context->backend->vTable->readSetting)
		return;

	if (context->backend->vTable->readInit)
		if (!(*context->backend->vTable->readInit) (context))
			return;

	PLUGIN_PRIV(plugin);
	CCSSettingList sl = pPrivate->settings;
	while (sl)
	{
		(*context->backend->vTable->readSetting) (context, sl->data);
		sl = sl->next;
	}
	if (context->backend->vTable->readDone)
		(*context->backend->vTable->readDone) (context);
}

void
ccsWriteSettings (CCSContext * context)
{
	if (!context || !context->backend)
		return;
	if (!context->backend->vTable->writeSetting)
		return;

	if (context->backend->vTable->writeInit)
		if (!(*context->backend->vTable->writeInit) (context))
			return;
	CCSPluginList pl = context->plugins;
	while (pl)
	{
		PLUGIN_PRIV(pl->data);
		CCSSettingList sl = pPrivate->settings;
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
		ccsSettingListFree (context->changedSettings, FALSE);
}

void
ccsWriteChangedSettings (CCSContext * context)
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
		CCSPluginList pl = context->plugins;
		while (pl)
		{
			CCSSetting *s =
				ccsFindSetting (pl->data, "____plugin_enabled", FALSE, 0);
			if (s)
				(*context->backend->vTable->writeSetting) (context, s);
			pl = pl->next;
		}
	}
	if (ccsSettingListLength (context->changedSettings))
	{
		CCSSettingList l = context->changedSettings;
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
		ccsSettingListFree (context->changedSettings, FALSE);
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
ccsIsEqualColor (CCSSettingColorValue c1, CCSSettingColorValue c2)
{
	EIGHTFIELDS (c1.color.red, c2.color.red, c1.color.blue, c2.color.blue,
				 c1.color.green, c2.color.green, c1.color.alpha,
				 c2.color.alpha) return 1;
}

Bool
ccsIsEqualAction (CCSSettingActionValue c1, CCSSettingActionValue c2)
{
	EIGHTFIELDS (c1.button, c2.button, c1.buttonModMask, c2.buttonModMask,
				 c1.keysym, c2.keysym, c1.keyModMask,
				 c2.keyModMask) FOURFIELDS (c1.onBell, c2.onBell, c1.edgeMask,
											c2.edgeMask) return 1;
}


Bool
ccsPluginSetActive (CCSPlugin * plugin, Bool value)
{
	if (!plugin)
		return FALSE;
	CCSSetting *s = ccsFindSetting (plugin, "____plugin_enabled", FALSE, 0);
	if (!s)
		return FALSE;
	ccsSetBool (s, value);

	return TRUE;
}

CCSPluginConflictList
ccsCanEnablePlugin (CCSContext * context, CCSPlugin * plugin)
{
	CCSPluginConflictList list = NULL;
	CCSPluginList pl, pl2;
	CCSStringList sl;

	/* look if the plugin to be loaded requires a plugin not present */
	sl = plugin->requiresPlugin;
	while (sl)
	{
		if (!ccsFindPlugin (context, sl->data))
		{
			NEW (CCSPluginConflict, conflict);
			conflict->value = strdup (sl->data);
			conflict->type = ConflictPluginError;
			conflict->plugins = NULL;
			list = ccsPluginConflictListAppend (list, conflict);
		}
		else if (!ccsPluginIsActive (context, sl->data))
		{
			/* we've not seen a matching plugin */
			NEW (CCSPluginConflict, conflict);
			conflict->value = strdup (sl->data);
			conflict->type = ConflictRequiresPlugin;
			conflict->plugins =
				ccsPluginListAppend (conflict->plugins,
									ccsFindPlugin (context, sl->data));
			list = ccsPluginConflictListAppend (list, conflict);
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
			CCSStringList featureList = pl->data->providesFeature;

			while (featureList)
			{
				if (strcmp (sl->data, featureList->data) == 0)
				{
					pl2 = ccsPluginListAppend (pl2, pl->data);
					break;
				}
				featureList = featureList->next;
			}

			pl = pl->next;
		}

		pl = pl2;

		while (pl)
		{
			if (ccsPluginIsActive (context, pl->data->name))
			{
				ccsPluginListFree (pl2, FALSE);
				break;
			}
			pl = pl->next;
		}

		if (!pl)
		{
			/* no plugin provides that feature */
			NEW (CCSPluginConflict, conflict);

			conflict->value = strdup (sl->data);
			conflict->type = ConflictRequiresFeature;
			conflict->plugins = pl2;

			list = ccsPluginConflictListAppend (list, conflict);
		}

		sl = sl->next;
	}

	/* look if another plugin provides the same feature */
	sl = plugin->providesFeature;
	while (sl)
	{
		pl = context->plugins;
		CCSPluginConflict *conflict = NULL;
		while (pl)
		{
			if (ccsPluginIsActive (context, pl->data->name))
			{
				CCSStringList featureList = pl->data->providesFeature;
				while (featureList)
				{
					if (strcmp (sl->data, featureList->data) == 0)
					{
						if (!conflict)
						{
							conflict = calloc (1, sizeof (CCSPluginConflict));
							conflict->value = strdup (sl->data);
							conflict->type = ConflictFeature;
						}

						conflict->plugins =
							ccsPluginListAppend (conflict->plugins, pl->data);
					}
					featureList = featureList->next;
				}
			}
			pl = pl->next;
		}

		if (conflict)
			list = ccsPluginConflictListAppend (list, conflict);

		sl = sl->next;
	}

	/* look if another plugin provides a conflicting feature*/
	sl = plugin->conflictFeature;
	while (sl)
	{
		pl = context->plugins;
		CCSPluginConflict *conflict = NULL;
		while (pl)
		{
			if (ccsPluginIsActive (context, pl->data->name))
			{
				CCSStringList featureList = pl->data->providesFeature;
				while (featureList)
				{
					if (strcmp (sl->data, featureList->data) == 0)
					{
						if (!conflict)
						{
							conflict = calloc (1, sizeof (CCSPluginConflict));
							conflict->value = strdup (sl->data);
							conflict->type = ConflictFeature;
						}

						conflict->plugins =
							ccsPluginListAppend (conflict->plugins, pl->data);
					}
					featureList = featureList->next;
				}
			}
			pl = pl->next;
		}

		if (conflict)
			list = ccsPluginConflictListAppend (list, conflict);

		sl = sl->next;
	}

	/* look if the plugin to be loaded conflict with a loaded plugin  */
	sl = plugin->conflictPlugin;
	while (sl)
	{
		if (ccsPluginIsActive (context, sl->data))
		{
			NEW (CCSPluginConflict, conflict);
			conflict->value = strdup (sl->data);
			conflict->type = ConflictPlugin;
			conflict->plugins =
				ccsPluginListAppend (conflict->plugins,
									ccsFindPlugin (context, sl->data));
			list = ccsPluginConflictListAppend (list, conflict);
		}
		sl = sl->next;
	}

	return list;
}

CCSPluginConflictList
ccsCanDisablePlugin (CCSContext * context, CCSPlugin * plugin)
{
	CCSPluginConflictList list = NULL;
	CCSPluginConflict *conflict = NULL;
	CCSPluginList pl;
	CCSStringList sl;

	/* look if the plugin to be unloaded is required by another plugin */
	pl = context->plugins;
	for (; pl; pl = pl->next)
	{
		CCSStringList pluginList;

		if (pl->data == plugin)
			continue;

		if (!ccsPluginIsActive (context, pl->data->name))
			continue;

		pluginList = pl->data->requiresPlugin;
		while (pluginList)
		{
			if (strcmp (plugin->name, pluginList->data) == 0)
			{
				if (!conflict)
				{
					conflict = calloc (1, sizeof (CCSPluginConflict));
					conflict->value = strdup (plugin->name);
					conflict->type = ConflictPluginNeeded;
				}
				conflict->plugins =
					ccsPluginListAppend (conflict->plugins, pl->data);

				break;
			}

			pluginList = pluginList->next;
		}
	}

	if (conflict)
	{
		list = ccsPluginConflictListAppend (list, conflict);
		conflict = NULL;
	}

	/* look if a feature provided is required by another plugin */
	sl = plugin->providesFeature;
	while (sl)
	{
		pl = context->plugins;

		for (; pl; pl = pl->next)
		{
			CCSStringList pluginList;

			if (pl->data == plugin)
				continue;

			if (!ccsPluginIsActive (context, pl->data->name))
				continue;

			pluginList = pl->data->requiresPlugin;
			while (pluginList)
			{
				if (strcmp (sl->data, pluginList->data) == 0)
				{
					if (!conflict)
					{
						conflict = calloc (1, sizeof (CCSPluginConflict));

						conflict->value = strdup (sl->data);
						conflict->type = ConflictPluginNeeded;
					}

					conflict->plugins =
						ccsPluginListAppend (conflict->plugins, pl->data);
				}

				pluginList = pluginList->next;
			}

			if (conflict)
				list = ccsPluginConflictListAppend (list, conflict);
		}

		sl = sl->next;
	}

	return list;
}

static Bool
conflictKey (CCSSettingActionValue a1, CCSSettingActionValue a2)
{
	if (a1.keysym == a2.keysym && a1.keyModMask == a2.keyModMask)
		return TRUE;
	return FALSE;
}

static Bool
conflictButton (CCSSettingActionValue a1, CCSSettingActionValue a2)
{
	if (a1.button == a2.button && a1.buttonModMask == a2.buttonModMask)
		return TRUE;
	return FALSE;
}

static Bool
conflictEdge (CCSSettingActionValue a1, CCSSettingActionValue a2)
{
	if (a1.edgeMask & a2.edgeMask)
		return TRUE;
	return FALSE;
}


CCSActionConflictList
ccsCanSetAction (CCSContext * context, CCSSettingActionValue action)
{
	CCSActionConflictList rv = NULL;
	NEW (CCSActionConflict, keyC);
	NEW (CCSActionConflict, buttonC);
	NEW (CCSActionConflict, edgeC);

	keyC->type = ConflictKey;
	buttonC->type = ConflictButton;
	edgeC->type = ConflictEdge;

	CCSPluginList pl = context->plugins;
	CCSSettingList sl = NULL;
	CCSSetting *s;

	while (pl)
	{
		PLUGIN_PRIV(pl->data);
		if (!pPrivate->loaded)
			ccsLoadPluginSettings(pl->data);
		sl = pPrivate->settings;
		while (sl)
		{
			s = sl->data;
			if (s->type == TypeAction)
			{
				if (conflictKey (action, s->value->value.asAction))
					keyC->settings = ccsSettingListAppend (keyC->settings, s);
				if (conflictButton (action, s->value->value.asAction))
					buttonC->settings =
						ccsSettingListAppend (buttonC->settings, s);
				if (conflictEdge (action, s->value->value.asAction))
					edgeC->settings =
						ccsSettingListAppend (edgeC->settings, s);
			}
			else if (s->type == TypeList
					 && s->info.forList.listType == TypeAction)
			{
				CCSSettingValueList vl = s->value->value.asList;
				while (vl)
				{
					if (conflictKey (action, vl->data->value.asAction))
						keyC->settings =
							ccsSettingListAppend (keyC->settings, s);
					if (conflictButton (action, vl->data->value.asAction))
						buttonC->settings =
							ccsSettingListAppend (buttonC->settings, s);
					if (conflictEdge (action, vl->data->value.asAction))
						edgeC->settings =
							ccsSettingListAppend (edgeC->settings, s);
					vl = vl->next;
				}
			}
			sl = sl->next;
		}
		pl = pl->next;
	}

	if (keyC->settings)
	{
		rv = ccsActionConflictListAppend (rv, keyC);
	}
	else
	{
		free (keyC);
	}

	if (buttonC->settings)
	{
		rv = ccsActionConflictListAppend (rv, buttonC);
	}
	else
	{
		free (buttonC);
	}

	if (edgeC->settings)
	{
		rv = ccsActionConflictListAppend (rv, edgeC);
	}
	else
	{
		free (edgeC);
	}

	return rv;
}

CCSStringList
ccsGetExistingProfiles (CCSContext * context)
{
	if (!context || !context->backend)
		return NULL;
	if (context->backend->vTable->getExistingProfiles)
		return context->backend->vTable->getExistingProfiles (context);
	return NULL;
}

void
ccsDeleteProfile (CCSContext * context, char *name)
{
	if (!context || !context->backend)
		return;

	/* never ever delete default profile */
	if (!name || !strlen (name))
	    return;

	/* if the current profile should be deleted, 
	   switch to default profile first */
	if (strcmp (context->profile, name) == 0)
	    ccsSetProfile (context, "");

	if (context->backend->vTable->deleteProfile)
		context->backend->vTable->deleteProfile (context, name);
}

static void
addBackendInfo (CCSBackendInfoList * bl, char *file)
{
	void *dlhand = NULL;
	char *err = NULL;
	Bool found = FALSE;
	dlerror ();

	dlhand = dlopen (file, RTLD_LAZY);
	err = dlerror ();

	if (err || !dlhand)
		return;

	BackendGetInfoProc getInfo = dlsym (dlhand, "getBackendInfo");

	if (!getInfo)
	{
		dlclose (dlhand);
		return;
	}

	CCSBackendVTable *vt = getInfo ();

	if (!vt)
	{
		dlclose (dlhand);
		return;
	}

	CCSBackendInfoList l = *bl;
	while (l)
	{
		if (!strcmp (l->data->name, vt->name))
		{
			found = TRUE;
			break;
		}
		l = l->next;
	}
	if (found)
	{
		dlclose (dlhand);
		return;
	}

	NEW (CCSBackendInfo, info);
	info->name = strdup (vt->name);
	info->shortDesc = (vt->shortDesc) ? strdup (vt->shortDesc) : strdup ("");
	info->longDesc = (vt->longDesc) ? strdup (vt->longDesc) : strdup ("");
	info->integrationSupport = vt->integrationSupport;
	info->profileSupport = vt->profileSupport;

	*bl = ccsBackendInfoListAppend (*bl, info);
	dlclose (dlhand);
}

static int
backendNameFilter (const struct dirent *name)
{
	int length = strlen (name->d_name);

	if (length < 7)
		return 0;

	if (strncmp (name->d_name, "lib", 3) ||
		strncmp (name->d_name + length - 3, ".so", 3))
		return 0;

	return 1;
}

static void
getBackendInfoFromDir (CCSBackendInfoList * bl, char *path)
{
	struct dirent **nameList;
	int nFile, i;

	if (!path)
		return;

	nFile = scandir (path, &nameList, backendNameFilter, NULL);

	if (nFile <= 0)
		return;

	for (i = 0; i < nFile; i++)
	{
		char file[1024];
		sprintf (file, "%s/%s", path, nameList[i]->d_name);
		addBackendInfo (bl, file);
		free (nameList[i]);
	}
	free (nameList);

}

CCSBackendInfoList
ccsGetExistingBackends ()
{
	CCSBackendInfoList rv = NULL;
	char *home = getenv ("HOME");
	char *backenddir;
	if (home && strlen (home))
	{
		asprintf (&backenddir, "%s/.compizconfig/backends", home);
		getBackendInfoFromDir (&rv, backenddir);
		free (backenddir);
	}

	asprintf (&backenddir, "%s/compizconfig/backends", LIBDIR);
	getBackendInfoFromDir (&rv, backenddir);
	free (backenddir);
	return rv;
}

Bool
ccsExportToFile (CCSContext * context, const char * fileName)
{
    IniDictionary *exportFile;
    CCSPluginList p;
    CCSSettingList s;
    CCSPlugin *plugin;
    CCSSetting *setting;
    char *keyName;

    exportFile = ccsIniNew ();
    if (!exportFile)
	return FALSE;

    for (p = context->plugins; p; p = p->next)
    {
	plugin = p->data;
	PLUGIN_PRIV(plugin);
	if (!pPrivate->loaded)
		ccsLoadPluginSettings (plugin);
	for (s = pPrivate->settings; s; s = s->next)
	{
	    setting = s->data;

	    if (setting->isScreen)
		asprintf (&keyName, "s%d_%s", setting->screenNum, setting->name);
    	    else
		asprintf (&keyName, "as_%s", setting->name);

	    switch (setting->type)
	    {
		case TypeBool:
		    ccsIniSetBool (exportFile, plugin->name, keyName, 
				  setting->value->value.asBool);
		    break;
		case TypeInt:
		    ccsIniSetInt (exportFile, plugin->name, keyName,
				 setting->value->value.asInt);
		    break;
		case TypeString:
		    ccsIniSetString (exportFile, plugin->name, keyName,
				    setting->value->value.asString);
		    break;
		case TypeAction:
		    ccsIniSetAction (exportFile, plugin->name, keyName,
				    setting->value->value.asAction);
		    break;
		case TypeColor:
		    ccsIniSetColor (exportFile, plugin->name, keyName,
				   setting->value->value.asColor);
		    break;
		case TypeMatch:
		    ccsIniSetString (exportFile, plugin->name, keyName,
				    setting->value->value.asMatch);
		    break;
		case TypeList:
		    ccsIniSetList (exportFile, plugin->name, keyName,
				  setting->value->value.asList, 
				  setting->info.forList.listType);
		    break;
		default:
		    break;
	    }

	    free (keyName);
	}
    }

    ccsIniSave (exportFile, fileName);
    ccsIniClose (exportFile);

    return TRUE;
}

Bool
ccsImportFromFile (CCSContext * context, const char * fileName, Bool overwrite)
{
    IniDictionary *importFile;
    CCSPluginList p;
    CCSSettingList s;
    CCSPlugin *plugin;
    CCSSetting *setting;
    char *keyName;

    importFile = ccsIniOpen (fileName);
    if (!importFile)
	return FALSE;

    for (p = context->plugins; p; p = p->next)
    {
	plugin = p->data;
	PLUGIN_PRIV(plugin);
	if (!pPrivate->loaded)
		ccsLoadPluginSettings (plugin);
	for (s = pPrivate->settings; s; s = s->next)
	{
	    setting = s->data;

	    if (!setting->isDefault && !overwrite)
		continue;

	    if (setting->isScreen)
		asprintf (&keyName, "s%d_%s", setting->screenNum, setting->name);
    	    else
		asprintf (&keyName, "as_%s", setting->name);

	    switch (setting->type)
	    {
		case TypeBool:
		    {
			Bool value;
			if (ccsIniGetBool (importFile, plugin->name, keyName, &value))
			    ccsSetBool (setting, value);
		    }
		    break;
		case TypeInt:
		    {
			int value;
			if (ccsIniGetInt (importFile, plugin->name, keyName, &value))
			    ccsSetInt (setting, value);
		    }
		    break;
		case TypeString:
		    {
			char *value;
			if (ccsIniGetString (importFile, plugin->name, keyName, &value))
			    ccsSetString (setting, value);
		    }
		    break;
		case TypeAction:
		    {
			CCSSettingActionValue value;
			if (ccsIniGetAction (importFile, plugin->name, keyName, &value))
			    ccsSetAction (setting, value);
		    }
		    break;
		case TypeColor:
		    {
			CCSSettingColorValue value;
			if (ccsIniGetColor (importFile, plugin->name, keyName, &value))
			    ccsSetColor (setting, value);
		    }
		    break;
		case TypeMatch:
		    {
			char *value;
			if (ccsIniGetString (importFile, plugin->name, keyName, &value))
			    ccsSetMatch (setting, value);
		    }
		    break;
		case TypeList:
		    {
			CCSSettingValueList value;
			if (ccsIniGetList (importFile, plugin->name, keyName, &value, setting))
			{
			    ccsSetList (setting, value);
			    ccsSettingValueListFree (value, TRUE);
			}
		    }
		    break;
		default:
		    break;
	    }

	    free (keyName);
	}
    }

    ccsIniClose (importFile);

    return TRUE;
}

CCSSettingList ccsGetPluginSettings(CCSPlugin *plugin)
{
	PLUGIN_PRIV(plugin);
	if (!pPrivate->loaded)
		ccsLoadPluginSettings(plugin);
	return pPrivate->settings;
}

CCSGroupList ccsGetPluginGroups(CCSPlugin *plugin)
{
	PLUGIN_PRIV(plugin);
	if (!pPrivate->loaded)
		ccsLoadPluginSettings(plugin);
	return pPrivate->groups;
}

Bool ccsSettingIsIntegrated (CCSSetting *setting)
{
	CCSBackend *b;
	if (!setting)
		return FALSE;
	b = setting->parent->context->backend;

	if (b && b->vTable->getSettingIsIntegrated)
		return b->vTable->getSettingIsIntegrated (setting);
	return FALSE;
}

Bool ccsSettingIsReadOnly (CCSSetting *setting)
{
	CCSBackend *b;
	if (!setting)
		return FALSE;
	b = setting->parent->context->backend;

	if (b && b->vTable->getSettingIsReadOnly)
		return b->vTable->getSettingIsReadOnly (setting);
	return FALSE;
}

