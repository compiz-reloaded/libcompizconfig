#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <compiz.h>

#include <bsettings.h>

static int displayPrivateIndex;

typedef struct _BSDisplay {
    int screenPrivateIndex;

	BSContext *context;

    CompTimeoutHandle timeoutHandle;

    InitPluginForDisplayProc      initPluginForDisplay;
    SetDisplayOptionProc	  setDisplayOption;
    SetDisplayOptionForPluginProc setDisplayOptionForPlugin;
} BSDisplay;

typedef struct _BSScreen {
    InitPluginForScreenProc      initPluginForScreen;
    SetScreenOptionProc		 setScreenOption;
    SetScreenOptionForPluginProc setScreenOptionForPlugin;
} BSScreen;

#define GET_BS_DISPLAY(d)				      \
    ((BSDisplay *) (d)->privates[displayPrivateIndex].ptr)

#define BS_DISPLAY(d)		     \
    BSDisplay *bd = GET_BS_DISPLAY (d)

#define GET_BS_SCREEN(s, bd)				         \
    ((BSScreen *) (s)->privates[(bd)->screenPrivateIndex].ptr)

#define BS_SCREEN(s)						           \
    BSScreen *bs = GET_BS_SCREEN (s, GET_BS_DISPLAY (s->display))

#define BS_UPDATE_TIMEOUT 250


static void
bsSetValueToValue(CompDisplay *d, BSSettingValue *sv, CompOptionValue *v, BSSettingType type)
{
	switch(type)
	{
		case TypeInt:
			v->i = sv->value.asInt;
			break;
		case TypeFloat:
			v->f = sv->value.asFloat;
			break;
		case TypeBool:
			v->b = sv->value.asBool;
			break;
		case TypeColor:
			{
				int i;
				for (i=0;i<4;i++)
					v->c[i] = sv->value.asColor.array.array[i];
			}
			break;
		case TypeString:
				v->s = strdup(sv->value.asString);
			break;
		case TypeMatch:
			matchInit (&v->match);
			matchAddFromString (&v->match, sv->value.asMatch);
			break;
		case TypeAction:
			{
				v->action.button.button = sv->value.asAction.button;
				v->action.button.modifiers = sv->value.asAction.buttonModMask;
				if ((v->action.button.button || v->action.button.modifiers)
				   && sv->parent->info.forAction.button)
					v->action.type |= CompBindingTypeButton;
				else
					v->action.type &= ~CompBindingTypeButton;

				v->action.key.keycode = XKeysymToKeycode(d->display, sv->value.asAction.keysym);
				v->action.key.modifiers = sv->value.asAction.keyModMask;
				if ((v->action.key.keycode || v->action.key.modifiers)
				   && sv->parent->info.forAction.key)
					v->action.type |= CompBindingTypeKey;
				else
					v->action.type &= ~CompBindingTypeKey;

				v->action.bell = sv->value.asAction.onBell;
				v->action.edgeMask = sv->value.asAction.edgeMask;
				v->action.edgeButton = sv->value.asAction.edgeButton;
				if (v->action.edgeButton)
					v->action.type |= CompBindingTypeEdgeButton;
				else
					v->action.type &= ~CompBindingTypeEdgeButton;
			}
			break;
		default:
			break;
	}
}

static void
bsSettingToValue (CompDisplay *d, BSSetting *s, CompOptionValue *v)
{

	if (s->type != TypeList)
		bsSetValueToValue(d, s->value, v, s->type);
	else
	{
		BSSettingValueList list;
		int i = 0;

		bsGetList(s, &list);

		v->list.nValue = bsSettingValueListLength(list);
		v->list.value  = malloc(v->list.nValue * sizeof(CompOptionValue));
		while (list)
		{
			bsSetValueToValue(d, list->data, &v->list.value[i], s->info.forList.listType);
			list = list->next;
			i++;
		}
	}
}

static void bsInitValue(CompDisplay *d, BSSettingValue * value, CompOptionValue * from,
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
			if (from->action.type & CompBindingTypeButton)
			{
				value->value.asAction.button = from->action.button.button;
				value->value.asAction.buttonModMask =
						from->action.button.modifiers;
			}
			else
			{
				value->value.asAction.button = 0;
				value->value.asAction.buttonModMask = 0;
			}

			if (from->action.type & CompBindingTypeKey)
			{
				value->value.asAction.keysym = XKeycodeToKeysym(d->display, from->action.key.keycode, 0);
				value->value.asAction.keyModMask = from->action.key.modifiers;
			}
			else
			{
				value->value.asAction.keysym = 0;
				value->value.asAction.keyModMask = 0;
			}

			value->value.asAction.onBell = from->action.bell;
			value->value.asAction.edgeMask = from->action.edgeMask;

			if (from->action.type & CompBindingTypeEdgeButton)
				value->value.asAction.edgeButton = from->action.edgeButton;
			else
				value->value.asAction.edgeButton = 0;

			break;
		default:
			break;
	}
}

static void
bsValueToSetting (CompDisplay *d, BSSetting *s, CompOptionValue *v)
{
	NEW(BSSettingValue, value);
	value->parent = s;

	if (s->type == TypeList)
	{
		int i;
		for (i = 0; i < v->list.nValue; i++)
		{
			NEW(BSSettingValue, val);
			val->parent = s;
			val->isListChild = TRUE;
			bsInitValue(d, val, &v->list.value[i],
					  s->info.forList.listType);
			value->value.asList = bsSettingValueListAppend(
					value->value.asList, val);
		}
	}
	else
		bsInitValue(d, value, v, s->type);

	bsSetValue(s, value);
	bsFreeSettingValue(value);
}

static void
bsFreeValue(CompOptionValue *v, BSSettingType type)
{
	switch(type)
	{
		case TypeString:
				free(v->s);
			break;
		case TypeMatch:
			matchFini (&v->match);
			break;
		default:
			break;
	}
}

static void
bsFreeCompValue (BSSetting *s, CompOptionValue *v)
{

	if (s->type != TypeList)
		bsFreeValue(v, s->type);
	else
	{
		int i = 0;
		for (i = 0; i < v->list.nValue; i++)
			bsFreeValue(&v->list.value[i], s->info.forList.listType);
		free(v->list.value);
	}
}

static void
bsUpdatePluginList(CompDisplay *d)
{
	CompOption *option;
	int	       nOption;
	CompOptionValue value;
	CompOption *o;
	int len,i;
	BSStringList list,l;
	BS_DISPLAY(d);

	option = compGetDisplayOptions (d, &nOption);
	o = compFindOption (option, nOption, "active_plugins", 0);

	if (!o)
		return;

	list = l = bsGetSortedPluginStringList(bd->context);

	len = bsStringListLength(list) + 1;
	value.list.nValue = len;
	value.list.value = malloc(len * sizeof(CompOptionValue));

	value.list.value[0].s = "bset";

	i = 1;
	while (l)
	{
		value.list.value[i].s = l->data;
		i++;
		l = l->next;
	}
	(*d->setDisplayOption) (d, "active_plugins", &value);

	free(value.list.value);
	bsStringListFree(list, TRUE);
}

static void
bsUpdateActivePlugins(CompDisplay *d, CompOption *o)
{
	BS_DISPLAY(d);

	BSPluginList l = bd->context->plugins;
	while (l)
	{
		Bool found = FALSE;
		int i;

		for (i = 0; i < o->value.list.nValue; i++)
			if (!strcmp(l->data->name, o->value.list.value[i].s))
				found = TRUE;

		bsPluginSetActive(l->data, found);

		l = l->next;
	}
}

static void
bsSetOptionFromContext( CompDisplay *d,
						char *plugin,
						char *name,
						Bool screen,
						int screenNum)
{
	CompPlugin *p = NULL;
	CompScreen *s = NULL;
	CompOption *o = NULL;
	CompOptionValue value;
	CompOption *option = NULL;
	int	       nOption;
	BSPlugin *bsp;
	BSSetting *setting;
	BS_DISPLAY(d);

	if (plugin && strlen(plugin))
	{
		p = findActivePlugin (plugin);
		if (!p)
			return;
	}

	if (!name)
		return;

	if (!p && !strcmp(name, "active_plugins"))
	{
		bsUpdatePluginList(d);
		return;
	}

	if (screen)
	{
		Bool found = FALSE;
		for (s = d->screens; s; s = s->next)
	    	if (s->screenNum == screenNum)
			{
				found = TRUE;
				break;
			}
		if (!found)
			return;
	}

	if (p)
	{
		if (s)
		{
			if (p->vTable->getScreenOptions)
				option = (*p->vTable->getScreenOptions) (p, s, &nOption);
		}
		else
		{
			if (p->vTable->getDisplayOptions)
				option = (*p->vTable->getDisplayOptions) (p, d, &nOption);
		}
	}
	else
	{
		if (s)
			option = compGetScreenOptions (s, &nOption);
		else
			option = compGetDisplayOptions (d, &nOption);
	}

	if (!option)
		return;

	o = compFindOption (option, nOption, name, 0);

	if (!o)
		return;

	bsp = bsFindPlugin(bd->context, plugin);

	if (!bsp)
		return;

	setting = bsFindSetting(bsp, name, screen, screenNum);

	if (!setting)
		return;

	value = o->value;

	bsSettingToValue(d, setting, &value);

	if (p)
	{
		if (s)
			(*s->setScreenOptionForPlugin) (s, plugin, name, &value);
		else
			(*d->setDisplayOptionForPlugin) (d, plugin, name, &value);
	}
	else
	{
		if (s)
			(*s->setScreenOption) (s, name, &value);
		else
			(*d->setDisplayOption) (d, name, &value);
	}

	bsFreeCompValue(setting, &value);
}

static void
bsSetContextFromOption( CompDisplay *d,
						char *plugin,
						char *name,
						Bool screen,
						int screenNum)
{
	CompPlugin *p = NULL;
	CompScreen *s = NULL;
	CompOption *o = NULL;
	CompOption *option = NULL;
	int	       nOption;
	BSPlugin *bsp;
	BSSetting *setting;
	BS_DISPLAY(d);

	if (plugin && strlen(plugin))
	{
		p = findActivePlugin (plugin);
		if (!p)
			return;
	}

	if (!name)
		return;

	if (screen)
	{
		Bool found = FALSE;
		for (s = d->screens; s; s = s->next)
	    	if (s->screenNum == screenNum)
			{
				found = TRUE;
				break;
			}
		if (!found)
			return;
	}

	if (p)
	{
		if (s)
		{
			if (p->vTable->getScreenOptions)
				option = (*p->vTable->getScreenOptions) (p, s, &nOption);
		}
		else
		{
			if (p->vTable->getDisplayOptions)
				option = (*p->vTable->getDisplayOptions) (p, d, &nOption);
		}
	}
	else
	{
		if (s)
			option = compGetScreenOptions (s, &nOption);
		else
			option = compGetDisplayOptions (d, &nOption);
	}

	if (!option)
		return;

	o = compFindOption (option, nOption, name, 0);

	if (!o)
		return;

	if (!p && !strcmp(name, "active_plugins"))
	{
		bsUpdateActivePlugins(d, o);
		return;
	}

	bsp = bsFindPlugin(bd->context, plugin);

	if (!bsp)
		return;

	setting = bsFindSetting(bsp, name, screen, screenNum);

	if (!setting)
		return;

	bsValueToSetting(d, setting, &o->value);

	bsWriteChangedSettings(bd->context);

}

static Bool
bsSetDisplayOption (CompDisplay     *d,
		       char	       *name,
		       CompOptionValue *value)
{
    Bool status;

    BS_DISPLAY (d);

    UNWRAP (bd, d, setDisplayOption);
    status = (*d->setDisplayOption) (d, name, value);
    WRAP (bd, d, setDisplayOption, bsSetDisplayOption);

    return status;
}

static Bool
bsSetDisplayOptionForPlugin (CompDisplay     *d,
				char	        *plugin,
				char	        *name,
				CompOptionValue *value)
{
    Bool status;

    BS_DISPLAY (d);

    UNWRAP (bd, d, setDisplayOptionForPlugin);
    status = (*d->setDisplayOptionForPlugin) (d, plugin, name, value);
    WRAP (bd, d, setDisplayOptionForPlugin, bsSetDisplayOptionForPlugin);

    return status;
}

static Bool
bsSetScreenOption (CompScreen      *s,
		      char	      *name,
		      CompOptionValue *value)
{
    Bool status;

    BS_SCREEN (s);

    UNWRAP (bs, s, setScreenOption);
    status = (*s->setScreenOption) (s, name, value);
    WRAP (bs, s, setScreenOption, bsSetScreenOption);

    return status;
}

static Bool
bsSetScreenOptionForPlugin (CompScreen      *s,
			       char	       *plugin,
			       char	       *name,
			       CompOptionValue *value)
{
    Bool status;

    BS_SCREEN (s);

    UNWRAP (bs, s, setScreenOptionForPlugin);
    status = (*s->setScreenOptionForPlugin) (s, plugin, name, value);
    WRAP (bs, s, setScreenOptionForPlugin, bsSetScreenOptionForPlugin);

	if (status)
    {
		bsSetContextFromOption (s->display, plugin,	name, TRUE, s->screenNum);
    }

    return status;
}

static Bool
bsInitPluginForDisplay (CompPlugin  *p,
			   CompDisplay *d)
{
    Bool status;

    BS_DISPLAY (d);

    UNWRAP (bd, d, initPluginForDisplay);
    status = (*d->initPluginForDisplay) (p, d);
    WRAP (bd, d, initPluginForDisplay, bsInitPluginForDisplay);

    if (status && p->vTable->getDisplayOptions)
    {
		CompOption *option;
		int	   nOption;
		int i;

		option = (*p->vTable->getDisplayOptions) (p, d, &nOption);

		for (i = 0; i < nOption; i++)
			bsSetOptionFromContext( d, p->vTable->name, option[i].name, FALSE, 0);
    }

    return status;
}

static Bool
bsInitPluginForScreen (CompPlugin *p,
			  CompScreen *s)
{
    Bool status;

    BS_SCREEN (s);

    UNWRAP (bs, s, initPluginForScreen);
    status = (*s->initPluginForScreen) (p, s);
    WRAP (bs, s, initPluginForScreen, bsInitPluginForScreen);

	if (status && p->vTable->getScreenOptions)
    {
		CompOption *option;
		int	   nOption;
		int i;

		option = (*p->vTable->getScreenOptions) (p, s, &nOption);
		for (i = 0; i < nOption; i++)
			bsSetOptionFromContext( s->display, p->vTable->name, option[i].name,
									TRUE, s->screenNum);
    }

    return status;
}

static Bool
bsTimeout (void *closure)
{
	CompDisplay *d = (CompDisplay *)closure;

	BS_DISPLAY(d);

	bsProcessEvents(bd->context);

	if (bsSettingListLength(bd->context->changedSettings))
	{
		BSSettingList l = bd->context->changedSettings;
		BSSetting *s;
		while (l)
		{
			s = l->data;
			bsSetOptionFromContext( d, s->parent->name, s->name,
									s->isScreen, s->screenNum);
			printf("Setting Update \"%s\"\n",s->name);
			l = l->next;
		}
		bd->context->changedSettings =
			bsSettingListFree(bd->context->changedSettings, FALSE);
	}
	if (bd->context->pluginsChanged)
	{
		bsUpdatePluginList(d);
		printf("Active Plugin List update\n");
		bd->context->pluginsChanged = FALSE;
	}

    return TRUE;
}

static Bool
bsInitDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    CompOption   *option;
    int	         nOption;
    BSDisplay *bd;
	int i;

    bd = malloc (sizeof (BSDisplay));
    if (!bd)
	return FALSE;

    bd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (bd->screenPrivateIndex < 0)
    {
	free (bd);
	return FALSE;
    }

    WRAP (bd, d, initPluginForDisplay, bsInitPluginForDisplay);
    WRAP (bd, d, setDisplayOption, bsSetDisplayOption);
    WRAP (bd, d, setDisplayOptionForPlugin, bsSetDisplayOptionForPlugin);

    d->privates[displayPrivateIndex].ptr = bd;

	bd->context = bsContextNew();
	if (!bd->context)
	{
		free(bd);
		return FALSE;
	}
	bsReadSettings(bd->context);
	bd->context->changedSettings =
			bsSettingListFree(bd->context->changedSettings, FALSE);


    option = compGetDisplayOptions (d, &nOption);

	for (i = 0; i < nOption; i++)
		bsSetOptionFromContext( d, NULL, option[i].name, FALSE, 0);

    bd->timeoutHandle = compAddTimeout (BS_UPDATE_TIMEOUT, bsTimeout, (void *)d);

    return TRUE;
}

static void
bsFiniDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    BS_DISPLAY (d);

    compRemoveTimeout (bd->timeoutHandle);

    UNWRAP (bd, d, initPluginForDisplay);
    UNWRAP (bd, d, setDisplayOption);
    UNWRAP (bd, d, setDisplayOptionForPlugin);

    freeScreenPrivateIndex (d, bd->screenPrivateIndex);

	bsContextDestroy(bd->context);

    free (bd);
}

static Bool
bsInitScreen (CompPlugin *p,
		 CompScreen *s)
{
    CompOption  *option;
    int	        nOption;
    BSScreen *bs;
	int i;


    BS_DISPLAY (s->display);

    bs = malloc (sizeof (BSScreen));
    if (!bs)
	return FALSE;

    WRAP (bs, s, initPluginForScreen, bsInitPluginForScreen);
    WRAP (bs, s, setScreenOption, bsSetScreenOption);
    WRAP (bs, s, setScreenOptionForPlugin, bsSetScreenOptionForPlugin);

    s->privates[bd->screenPrivateIndex].ptr = bs;

	option = compGetScreenOptions (s, &nOption);

	for (i = 0; i < nOption; i++)
		bsSetOptionFromContext( s->display, NULL, option[i].name,
								TRUE, s->screenNum);

    return TRUE;
}

static void
bsFiniScreen (CompPlugin *p,
		 CompScreen *s)
{
    BS_SCREEN (s);

    UNWRAP (bs, s, initPluginForScreen);
    UNWRAP (bs, s, setScreenOption);
    UNWRAP (bs, s, setScreenOptionForPlugin);

    free (bs);
}

static Bool
bsInit (CompPlugin *p)
{
    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
	return FALSE;

    return TRUE;
}

static void
bsFini (CompPlugin *p)
{
    if (displayPrivateIndex >= 0)
	freeDisplayPrivateIndex (displayPrivateIndex);
}

static int
bsGetVersion (CompPlugin *plugin,
		 int	    version)
{
    return ABIVERSION;
}

CompPluginVTable bsVTable = {
    "bset",
    "BSettings",
    "Beryl configuration system plugin",
    bsGetVersion,
    bsInit,
    bsFini,
    bsInitDisplay,
    bsFiniDisplay,
    bsInitScreen,
    bsFiniScreen,
    0, /* InitWindow */
    0, /* FiniWindow */
    0, /* GetDisplayOptions */
    0, /* SetDisplayOption */
    0, /* GetScreenOptions */
    0, /* SetScreenOption */
    0,
    0,
    0, /* Features */
    0  /* nFeatures */
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &bsVTable;
}
