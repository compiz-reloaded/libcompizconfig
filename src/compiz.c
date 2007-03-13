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

static void initOption(BSPlugin * plugin, CompOption * o, Bool isScreen)
{
	printf("Reading Option %s\n", o->name);
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
	/*
	//make sure a plugin by the same name isn't already loaded
	if (beryl_settings_context_find_plugin(context,vt->name))
	{
		dlclose(dlhand);
		return;
	}
	*/
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

	/*
	if (!beryl_settings_plugin_find_setting(plugin,"____plugin_enabled",FALSE))
	{
		NEW(BerylSetting,setting);
		setting->parent=plugin;
		setting->is_screen=FALSE;
		setting->is_default=TRUE;
		setting->name=g_strdup("____plugin_enabled");
		setting->short_desc=g_strdup("enabled");
		setting->long_desc=g_strdup("enabled");
		setting->group=g_strdup("");
		setting->subGroup=g_strdup("");
		setting->type=BERYL_SETTING_TYPE_BOOL;
		setting->advanced=TRUE;
		setting->default_value.parent=setting;
		setting->default_value.value.as_bool=vt->defaultEnabled;
		copy_from_default(setting);
		plugin->settings=g_slist_append(plugin->settings,setting);
	}
	else
	{
		g_error("Couldn't load plugin %s because it has a setting defined named ____plugin_enabled",plugin->name);
	}
	*/
	
	dlclose(dlhand);
	//collate_groups(plugin);
	context->plugins = bsPluginListAppend(context->plugins, plugin);
}



