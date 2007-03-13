#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <libintl.h>
#include <dlfcn.h>

#include <compiz.h>

#include <bsettings.h>

#include "bsettings-private.h"

static void initValue(BSSettingValue * value, CompOptionValue * from,
					  BSSettingType type)
{
	switch(type)
	{
		case TypeInt:
			value->value.asInt = from->i;
			break;
		case TypeFloat:
			value->value.asFloat = from->f;
			break;
		case TypeBool:
			value->value.asBool = from->b;
			break;
		case TypeColor:
			{
				int i;
				for (i=0;i<4;i++)
					value->value.asColor.array.array[i] = from->c[i];
			}
			break;
		case TypeString:
			value->value.asString = strdup(from->s);
			break;
		case TypeMatch:
			value->value.asMatch = matchToString(&from->match);
			break;
		case TypeAction:
			value->value.asAction.button = from->action.button.button;
			value->value.asAction.buttonModMask = from->action.button.modifiers;
			value->value.asAction.keysym = from->action.key.keysym;
			value->value.asAction.keyModMask = from->action.key.modifiers;
			value->value.asAction.onBell = from->action.bell;
			value->value.asAction.edgeMask = from->action.edgeMask;
			break;
		default:
			break;
	}
}

static void initInfo(BSSettingInfo * info, CompOption * o, BSSettingType type)
{
	switch(type)
	{
		case TypeInt:
			info->forInt.min = o->rest.i.min;
			info->forInt.max = o->rest.i.max;
			break;
		case TypeFloat:
			info->forFloat.min = o->rest.f.min;
			info->forFloat.max = o->rest.f.max;
			info->forFloat.precision = o->rest.f.precision;
			break;
		case TypeString:
			if (o->rest.s.nString && o->rest.s.string)
			{
				int i;
				for (i = 0; i < o->rest.s.nString; i++)
				{
					info->forString.allowedValues =
							bsStringListAppend(info->forString.allowedValues,
											   strdup(o->rest.s.string[i]));
				}
			}
			break;
		case TypeAction:
			info->forAction.key =
					(o->value.action.state & CompActionStateInitKey) ||
					(o->value.action.state & CompActionStateTermKey);
			info->forAction.button =
					(o->value.action.state & CompActionStateInitButton) ||
					(o->value.action.state & CompActionStateTermButton);
			info->forAction.edge =
					(o->value.action.state & CompActionStateInitEdge) ||
					(o->value.action.state & CompActionStateTermEdge);
			info->forAction.bell =
					(o->value.action.state & CompActionStateInitBell) && TRUE;
			break;
		default:
			break;
	}
}

static BSSettingType translateType(CompOptionType type)
{
	switch(type)
	{
		case CompOptionTypeInt:
			return TypeInt;
		case CompOptionTypeFloat:
			return TypeFloat;
		case CompOptionTypeString:
			return TypeString;
		case CompOptionTypeColor:
			return TypeColor;
		case CompOptionTypeBool:
			return TypeBool;
		case CompOptionTypeAction:
			return TypeAction;
		case CompOptionTypeList:
			return TypeList;
		case CompOptionTypeMatch:
			return TypeMatch;
		default:
			break;
	}
	return TypeNum;
}

static void initOption(BSPlugin * plugin, CompOption * o, Bool isScreen)
{
	printf("Reading Option %s\n", o->name);
	NEW(BSSetting, setting);
	
	setting->parent     = plugin;
	setting->isScreen   = isScreen;
	setting->screenNum  = 0;
	setting->isDefault  = TRUE;
	setting->name       = strdup(o->name);
	setting->shortDesc  = strdup(o->shortDesc);
	setting->longDesc   = strdup(o->longDesc);
	setting->group      = strdup("");
	setting->subGroup   = strdup("");
	setting->hints      = strdup("");
	setting->type       = translateType(o->type);
	
	setting->defaultValue.parent = setting;
	
	if (setting->type == TypeList)
	{
		NEW(BSSettingInfo, listInfo);
		setting->info.forList.listType = translateType(o->value.list.type);
		setting->info.forList.listInfo = listInfo;
		
		initInfo(setting->info.forList.listInfo, o,
				 setting->info.forList.listType);
		int i;
		for (i = 0; i < o->value.list.nValue; i++)
		{
			NEW(BSSettingValue, val);
			val->parent = setting;
			val->isListChild = TRUE;
			initValue(val, &o->value.list.value[i],
					  setting->info.forList.listType);
			setting->defaultValue.value.asList = bsSettingValueListAppend(
					setting->defaultValue.value.asList, val);
		}
	}
	else
	{
		initInfo(&setting->info, o, setting->type);
		initValue(&setting->defaultValue, &o->value, setting->type);
	}

	setting->value = &setting->defaultValue;
	
	plugin->settings = bsSettingListAppend(plugin->settings, setting);
	
}

void bsLoadPlugin(BSContext * context, char * filename)
{
	printf("Loading Plugin %s\n",filename);
	
	void * dlhand;
	char * err;
	int n,i;
	PluginGetInfoProc getInfo;
	CompPluginVTable * vt;
	dlhand = dlopen(filename,RTLD_LAZY);

	if (!dlhand)
	{
		err=dlerror();
		if (err)
			fprintf(stderr, _("libbsettings: dlopen: %s\n"),err);
		return;
	}
	dlerror();

	getInfo = (PluginGetInfoProc) dlsym (dlhand, "getCompPluginInfo");
	if ((err = dlerror()))
	{
		fprintf (stderr, _("libbsettings: dlsym: %s\n"), err);
		dlclose(dlhand);
		return;
	}
	vt = getInfo();
	if (vt->getVersion(NULL, 0) != ABIVERSION)
	{
		fprintf (stderr, _("libbsettings: Couldn't get vtable from '%s' plugin\n"), filename);
		dlclose (dlhand);
		return;
	}
	
	//make sure a plugin by the same name isn't already loaded
	if (bsFindPlugin(context, vt->name))
	{
		dlclose(dlhand);
		return;
	}
	
	NEW(BSPlugin,plugin);
	
	plugin->context = context;
	plugin->name    = strdup(vt->name);
	
	plugin->shortDesc = strdup(vt->shortDesc);
	plugin->longDesc  = strdup(vt->longDesc);
	plugin->filename  = strdup(filename);

	plugin->category  = NULL;
	
	
	for (n = 0; n < vt->nDeps; n++)
	{
		switch(vt->deps[n].rule)
		{
			case CompPluginRuleBefore:
				plugin->loadBefore = bsStringListAppend(plugin->loadBefore,
						strdup(vt->deps[n].name));
				break;
			case CompPluginRuleAfter:
				plugin->loadAfter = bsStringListAppend(plugin->loadAfter,
						strdup(vt->deps[n].name));
				break;
			case CompPluginRuleRequire:
				plugin->requires = bsStringListAppend(plugin->requires,
						strdup(vt->deps[n].name));
				break;
			default:
				break;
		}
	}
	
	for (n = 0; n < vt->nFeatures; n++)
	{
		plugin->provides = bsStringListAppend(plugin->provides,
				strdup(vt->features[n].name));
	}
	
	if (vt->getDisplayOptions)
	{
		CompOption * o;
		o = vt->getDisplayOptions(NULL, &n);
		for (i = 0; i < n; i++)
		{
			initOption(plugin, o++, FALSE);
		}
	}
	if (vt->getScreenOptions)
	{
		CompOption * o;
		o = vt->getScreenOptions(NULL, &n);
		for (i = 0; i < n; i++)
		{
			initOption(plugin, o++, TRUE);
		}
	}

	if (!bsFindSetting(plugin, "____plugin_enabled", FALSE, 0))
	{
		NEW(BSSetting, setting);
		
		setting->parent                    = plugin;
		setting->isScreen                  = FALSE;
		setting->screenNum                 = 0;
		setting->isDefault                 = TRUE;
		setting->name                      = strdup("____plugin_enabled");
		setting->shortDesc                 = strdup("enabled");
		setting->longDesc                  = strdup("enabled");
		setting->group                     = strdup("");
		setting->subGroup                  = strdup("");
		setting->type                      = TypeBool;
		setting->defaultValue.parent       = setting;
		setting->defaultValue.value.asBool = FALSE;
		
		setting->value = &setting->defaultValue;
	
		plugin->settings = bsSettingListAppend(plugin->settings, setting);
	}
	else
	{
		fprintf(stderr, "Couldn't load plugin %s because it has a setting "
						"defined named ____plugin_enabled", plugin->name);
	}
	
	dlclose(dlhand);
	//collate_groups(plugin);
	context->plugins = bsPluginListAppend(context->plugins, plugin);
}



