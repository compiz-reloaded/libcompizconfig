/*
 * Compiz configuration system library plugin
 *
 * Copyright (C) 2007  Dennis Kasprzyk <onestone@opencompositing.org>
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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <compiz.h>

#include <ccs.h>

static int displayPrivateIndex;

typedef struct _CCPDisplay
{
    int screenPrivateIndex;

    CCSContext *context;
    Bool applyingSettings;

    CompTimeoutHandle timeoutHandle;

    InitPluginForDisplayProc      initPluginForDisplay;
    SetDisplayOptionProc	  setDisplayOption;
    SetDisplayOptionForPluginProc setDisplayOptionForPlugin;
}

CCPDisplay;

typedef struct _CCPScreen
{
    InitPluginForScreenProc      initPluginForScreen;
    SetScreenOptionProc		 setScreenOption;
    SetScreenOptionForPluginProc setScreenOptionForPlugin;
}

CCPScreen;

#define GET_CCP_DISPLAY(d)				      \
    ((CCPDisplay *) (d)->privates[displayPrivateIndex].ptr)

#define CCP_DISPLAY(d)		     \
    CCPDisplay *cd = GET_CCP_DISPLAY (d)

#define GET_CCP_SCREEN(s, cd)				         \
    ((CCPScreen *) (s)->privates[(cd)->screenPrivateIndex].ptr)

#define CCP_SCREEN(s)						           \
    CCPScreen *cs = GET_CCP_SCREEN (s, GET_CCP_DISPLAY (s->display))

#define CCP_UPDATE_TIMEOUT 250
#define CORE_VTABLE_NAME  "core"

static void
ccpSetValueToValue (CompDisplay     *d,
		    CCSSettingValue *sv,
		    CompOptionValue *v,
		    CCSSettingType  type)
{
    switch (type)
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

	    for (i = 0; i < 4; i++)
		v->c[i] = sv->value.asColor.array.array[i];
	}
	break;
    case TypeString:
	v->s = strdup (sv->value.asString);
	break;
    case TypeMatch:
	matchInit (&v->match);
	matchAddFromString (&v->match, sv->value.asMatch);
	break;
    case TypeAction:
	{
	    v->action.button.button = sv->value.asAction.button;
	    v->action.button.modifiers = sv->value.asAction.buttonModMask;

	    if ((v->action.button.button || v->action.button.modifiers) &&
		sv->parent->info.forAction.button)
		v->action.type |= CompBindingTypeButton;
	    else
		v->action.type &= ~CompBindingTypeButton;

	    v->action.key.keycode =
		(sv->value.asAction.keysym != NoSymbol) ?
		XKeysymToKeycode (d->display, sv->value.asAction.keysym) : 0;

	    v->action.key.modifiers = sv->value.asAction.keyModMask;

	    if ((v->action.key.keycode || v->action.key.modifiers) &&
		sv->parent->info.forAction.key)
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
ccpSettingToValue (CompDisplay     *d,
		   CCSSetting      *s,
		   CompOptionValue *v)
{
    if (s->type != TypeList)
	ccpSetValueToValue (d, s->value, v, s->type);
    else
    {
	CCSSettingValueList list;
	int                 i = 0;

	ccsGetList (s, &list);

	v->list.nValue = ccsSettingValueListLength (list);
	v->list.value  = malloc (v->list.nValue * sizeof (CompOptionValue));

	while (list)
	{
	    ccpSetValueToValue (d, list->data,
				&v->list.value[i], s->info.forList.listType);
	    list = list->next;
	    i++;
	}
    }
}

static void
ccpInitValue (CompDisplay     *d,
	      CCSSettingValue *value,
	      CompOptionValue *from,
	      CCSSettingType  type)
{
    switch (type)
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

	    for (i = 0; i < 4; i++)
		value->value.asColor.array.array[i] = from->c[i];
	}
	break;
    case TypeString:
	value->value.asString = strdup (from->s);
	break;
    case TypeMatch:
	value->value.asMatch = matchToString (&from->match);
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
	    value->value.asAction.keysym = 
		XKeycodeToKeysym (d->display, from->action.key.keycode, 0);
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
ccpValueToSetting (CompDisplay     *d,
		   CCSSetting      *s,
		   CompOptionValue *v)
{
    NEW (CCSSettingValue, value);
    value->parent = s;

    if (s->type == TypeList)
    {
	int i;

	for (i = 0; i < v->list.nValue; i++)
	{
	    NEW (CCSSettingValue, val);
	    val->parent = s;
	    val->isListChild = TRUE;
	    ccpInitValue (d, val, &v->list.value[i],
			  s->info.forList.listType);
	    value->value.asList =
		ccsSettingValueListAppend (value->value.asList, val);
	}
    }
    else
	ccpInitValue (d, value, v, s->type);

    ccsSetValue (s, value);
    ccsFreeSettingValue (value);
}

static void
ccpFreeValue (CompOptionValue *v,
	      CCSSettingType  type)
{
    switch (type)
    {
    case TypeString:
	free (v->s);
	break;
    case TypeMatch:
	matchFini (&v->match);
	break;
    default:
	break;
    }
}

static void
ccpFreeCompValue (CCSSetting      *s,
		  CompOptionValue *v)
{
    if (s->type != TypeList)
	ccpFreeValue (v, s->type);
    else
    {
	int i = 0;

	for (i = 0; i < v->list.nValue; i++)
	    ccpFreeValue (&v->list.value[i], s->info.forList.listType);

	free (v->list.value);
    }
}

static void
ccpUpdatePluginList (CompDisplay *d)
{
    CompOption      *option;
    int	            nOption;
    CompOptionValue value;
    CompOption      *o;
    int             len, i;
    CCSStringList   list, l;

    CCP_DISPLAY (d);

    option = compGetDisplayOptions (d, &nOption);
    o = compFindOption (option, nOption, "active_plugins", 0);
    if (!o)
	return;

    list = l = ccsGetSortedPluginStringList (cd->context);
    len = ccsStringListLength (list) + 1;

    value.list.nValue = len;
    value.list.value = malloc (len * sizeof (CompOptionValue));
    value.list.value[0].s = "ccp";

    i = 1;

    while (l)
    {
	value.list.value[i].s = l->data;
	i++;
	l = l->next;
    }

    (*d->setDisplayOption) (d, "active_plugins", &value);

    free (value.list.value);
    ccsStringListFree (list, TRUE);
}

static void
ccpUpdateActivePlugins (CompDisplay *d, CompOption *o)
{
    CCP_DISPLAY (d);

    CCSPluginList l = cd->context->plugins;

    while (l)
    {
	Bool found = FALSE;
	int i;

	for (i = 0; i < o->value.list.nValue; i++)
	    if (!strcmp (l->data->name, o->value.list.value[i].s))
		found = TRUE;

	ccsPluginSetActive (l->data, found);

	l = l->next;
    }
}

static void
ccpSetOptionFromContext ( CompDisplay *d,
			  char *plugin,
			  char *name,
			  Bool screen,
			  int screenNum)
{
    CompPlugin      *p = NULL;
    CompScreen      *s = NULL;
    CompOption      *o = NULL;
    CompOptionValue value;
    CompOption      *option = NULL;
    int	            nOption;
    CCSPlugin      *bsp;
    CCSSetting      *setting;

    CCP_DISPLAY (d);

    if (plugin && strlen (plugin) && (strcmp (plugin, CORE_VTABLE_NAME) != 0))
    {
	p = findActivePlugin (plugin);
	if (!p)
	    return;
    }

    if (!name)
	return;

    if (!p && !strcmp (name, "active_plugins"))
    {
	ccpUpdatePluginList (d);
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

    bsp = ccsFindPlugin (cd->context, (plugin) ? plugin : CORE_VTABLE_NAME);
    if (!bsp)
	return;

    setting = ccsFindSetting (bsp, name, screen, screenNum);
    if (!setting)
	return;

    value = o->value;
    ccpSettingToValue (d, setting, &value);

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

    ccpFreeCompValue (setting, &value);
}

static void
ccpSetContextFromOption ( CompDisplay *d,
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
    CCSPlugin *bsp;
    CCSSetting *setting;

    CCP_DISPLAY (d);

    if (plugin && strlen (plugin))
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

    if (!p && !strcmp (name, "active_plugins"))
    {
	ccpUpdateActivePlugins (d, o);
	return;
    }

    bsp = ccsFindPlugin (cd->context, (plugin) ? plugin : CORE_VTABLE_NAME);
    if (!bsp)
	return;

    setting = ccsFindSetting (bsp, name, screen, screenNum);
    if (!setting)
	return;

    ccpValueToSetting (d, setting, &o->value);
    ccsWriteChangedSettings (cd->context);
}

static Bool
ccpSetDisplayOption (CompDisplay     *d,
		     char	     *name,
		     CompOptionValue *value)
{
    Bool status;

    CCP_DISPLAY (d);

    UNWRAP (cd, d, setDisplayOption);
    status = (*d->setDisplayOption) (d, name, value);
    WRAP (cd, d, setDisplayOption, ccpSetDisplayOption);

    if (status && !cd->applyingSettings)
    {
	ccpSetContextFromOption (d, NULL, name, FALSE, 0);
    }

    return status;
}

static Bool
ccpSetDisplayOptionForPlugin (CompDisplay     *d,
			      char	      *plugin,
			      char	      *name,
			      CompOptionValue *value)
{
    Bool status;

    CCP_DISPLAY (d);

    UNWRAP (cd, d, setDisplayOptionForPlugin);
    status = (*d->setDisplayOptionForPlugin) (d, plugin, name, value);
    WRAP (cd, d, setDisplayOptionForPlugin, ccpSetDisplayOptionForPlugin);

    if (status && !cd->applyingSettings)
    {
	ccpSetContextFromOption (d, plugin, name, FALSE, 0);
    }

    return status;
}

static Bool
ccpSetScreenOption (CompScreen      *s,
		    char	    *name,
		    CompOptionValue *value)
{
    Bool status;

    CCP_SCREEN (s);

    UNWRAP (cs, s, setScreenOption);
    status = (*s->setScreenOption) (s, name, value);
    WRAP (cs, s, setScreenOption, ccpSetScreenOption);

    if (status)
    {
	CCP_DISPLAY (s->display);

	if (!cd->applyingSettings)
	    ccpSetContextFromOption (s->display, NULL, name, 
				     TRUE, s->screenNum);
    }

    return status;
}

static Bool
ccpSetScreenOptionForPlugin (CompScreen      *s,
			     char	     *plugin,
			     char	     *name,
			     CompOptionValue *value)
{
    Bool status;

    CCP_SCREEN (s);

    UNWRAP (cs, s, setScreenOptionForPlugin);
    status = (*s->setScreenOptionForPlugin) (s, plugin, name, value);
    WRAP (cs, s, setScreenOptionForPlugin, ccpSetScreenOptionForPlugin);

    if (status)
    {
	CCP_DISPLAY (s->display);

	if (!cd->applyingSettings)
	    ccpSetContextFromOption (s->display, plugin,
				     name, TRUE, s->screenNum);
    }

    return status;
}

static Bool
ccpInitPluginForDisplay (CompPlugin  *p,
			 CompDisplay *d)
{
    Bool status;

    CCP_DISPLAY (d);

    UNWRAP (cd, d, initPluginForDisplay);
    status = (*d->initPluginForDisplay) (p, d);
    WRAP (cd, d, initPluginForDisplay, ccpInitPluginForDisplay);

    if (status && p->vTable->getDisplayOptions)
    {
	CompOption *option;
	int	   nOption;
	int        i;

	option = (*p->vTable->getDisplayOptions) (p, d, &nOption);

	for (i = 0; i < nOption; i++)
	    ccpSetOptionFromContext (d, p->vTable->name,
				     option[i].name, FALSE, 0);
    }

    return status;
}

static Bool
ccpInitPluginForScreen (CompPlugin *p,
			CompScreen *s)
{
    Bool status;

    CCP_SCREEN (s);

    UNWRAP (cs, s, initPluginForScreen);
    status = (*s->initPluginForScreen) (p, s);
    WRAP (cs, s, initPluginForScreen, ccpInitPluginForScreen);

    if (status && p->vTable->getScreenOptions)
    {
	CompOption *option;
	int	   nOption;
	int i;

	option = (*p->vTable->getScreenOptions) (p, s, &nOption);

	for (i = 0; i < nOption; i++)
	    ccpSetOptionFromContext (s->display, p->vTable->name,
				     option[i].name,
				     TRUE, s->screenNum);
    }

    return status;
}

static Bool
ccpTimeout (void *closure)
{
    CompDisplay *d = (CompDisplay *) closure;
    unsigned int flags = 0;

    CCP_DISPLAY (d);

    if (findActivePlugin ("glib"))
	flags |= ProcessEventsNoGlibMainLoopMask;

    ccsProcessEvents (cd->context, flags);

    if (ccsSettingListLength (cd->context->changedSettings))
    {
	CCSSettingList list = cd->context->changedSettings;
	CCSSettingList l = list;
	cd->context->changedSettings = NULL;
	CCSSetting *s;

	cd->applyingSettings = TRUE;

	while (l)
	{
	    s = l->data;
	    ccpSetOptionFromContext (d, s->parent->name, s->name,
				     s->isScreen, s->screenNum);
	    printf ("Setting Update \"%s\"\n", s->name);
	    l = l->next;
	}

	cd->applyingSettings = FALSE;

	ccsSettingListFree (list, FALSE);
	cd->context->changedSettings =
	    ccsSettingListFree (cd->context->changedSettings, FALSE);
    }

    if (cd->context->pluginsChanged)
    {
	cd->applyingSettings = TRUE;
	ccpUpdatePluginList (d);
	cd->applyingSettings = FALSE;
	printf ("Active Plugin List update\n");
	cd->context->pluginsChanged = FALSE;
    }

    return TRUE;
}

static Bool
ccpInitDisplay (CompPlugin  *p,
		CompDisplay *d)
{
    CompOption   *option;
    int	         nOption;
    CCPDisplay *cd;
    CompScreen *s;
    int i;
    unsigned int *screens;

    cd = malloc (sizeof (CCPDisplay));

    if (!cd)
	return FALSE;

    cd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (cd->screenPrivateIndex < 0)
    {
	free (cd);
	return FALSE;
    }

    WRAP (cd, d, initPluginForDisplay, ccpInitPluginForDisplay);
    WRAP (cd, d, setDisplayOption, ccpSetDisplayOption);
    WRAP (cd, d, setDisplayOptionForPlugin, ccpSetDisplayOptionForPlugin);

    d->privates[displayPrivateIndex].ptr = cd;

    cd->applyingSettings = FALSE;

    s = d->screens;
    i = 0;

    while (s)
    {
	i++;
	s = s->next;
    }

    screens = calloc (1, sizeof (unsigned int) * i);

    s = d->screens;
    i = 0;

    while (s)
    {
	screens[i] = s->screenNum;
	i++;
	s = s->next;
    }

    ccsSetBasicMetadata (TRUE);

    cd->context = ccsContextNew (screens, i);

    free (screens);

    if (!cd->context)
    {
	free (cd);
	return FALSE;
    }

    ccsReadSettings (cd->context);

    cd->context->changedSettings =
	ccsSettingListFree (cd->context->changedSettings, FALSE);

    option = compGetDisplayOptions (d, &nOption);

    for (i = 0; i < nOption; i++)
	ccpSetOptionFromContext ( d, NULL, option[i].name, FALSE, 0);

    cd->timeoutHandle = compAddTimeout (CCP_UPDATE_TIMEOUT, 
					ccpTimeout, (void *) d);

    return TRUE;
}

static void
ccpFiniDisplay (CompPlugin  *p,
		CompDisplay *d)
{
    CCP_DISPLAY (d);

    compRemoveTimeout (cd->timeoutHandle);

    UNWRAP (cd, d, initPluginForDisplay);
    UNWRAP (cd, d, setDisplayOption);
    UNWRAP (cd, d, setDisplayOptionForPlugin);

    freeScreenPrivateIndex (d, cd->screenPrivateIndex);

    ccsContextDestroy (cd->context);

    free (cd);
}

static Bool
ccpInitScreen (CompPlugin *p,
	       CompScreen *s)
{
    CompOption  *option;
    int	        nOption;
    CCPScreen *cs;
    int i;


    CCP_DISPLAY (s->display);

    cs = malloc (sizeof (CCPScreen));

    if (!cs)
	return FALSE;

    WRAP (cs, s, initPluginForScreen, ccpInitPluginForScreen);
    WRAP (cs, s, setScreenOption, ccpSetScreenOption);
    WRAP (cs, s, setScreenOptionForPlugin, ccpSetScreenOptionForPlugin);

    s->privates[cd->screenPrivateIndex].ptr = cs;

    option = compGetScreenOptions (s, &nOption);

    for (i = 0; i < nOption; i++)
	ccpSetOptionFromContext (s->display, NULL, option[i].name,
				 TRUE, s->screenNum);

    return TRUE;
}

static void
ccpFiniScreen (CompPlugin *p,
	       CompScreen *s)
{
    CCP_SCREEN (s);

    UNWRAP (cs, s, initPluginForScreen);
    UNWRAP (cs, s, setScreenOption);
    UNWRAP (cs, s, setScreenOptionForPlugin);

    free (cs);
}

static Bool
ccpInit (CompPlugin *p)
{
    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
	return FALSE;

    return TRUE;
}

static void
ccpFini (CompPlugin *p)
{
    if (displayPrivateIndex >= 0)
	freeDisplayPrivateIndex (displayPrivateIndex);
}

static int
ccpGetVersion (CompPlugin *plugin,
	       int	    version)
{
    return ABIVERSION;
}

CompPluginVTable ccpVTable = {

    "ccp",
    ccpGetVersion,
    0,
    ccpInit,
    ccpFini,
    ccpInitDisplay,
    ccpFiniDisplay,
    ccpInitScreen,
    ccpFiniScreen,
    0, /* InitWindow */
    0, /* FiniWindow */
    0, /* GetDisplayOptions */
    0, /* SetDisplayOption */
    0, /* GetScreenOptions */
    0, /* SetScreenOption */
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &ccpVTable;
}
