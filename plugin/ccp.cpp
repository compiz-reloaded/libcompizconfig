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

#include "ccp.h"

COMPIZ_PLUGIN_20081216 (ccp, CcpPluginVTable)

#define CCP_UPDATE_MIN_TIMEOUT 250
#define CCP_UPDATE_MAX_TIMEOUT 4000
#define CORE_VTABLE_NAME  "core"


	
static void
ccpSetValueToValue (CCSSettingValue *sv,
		    CompOption::Value *v,
		    CCSSettingType  type)
{
    switch (type)
    {
	case TypeInt:
	    v->set ((int) sv->value.asInt);
	    break;
	case TypeFloat:
	    v->set ((float) sv->value.asFloat);
	    break;
	case TypeBool:
	    v->set ((bool) sv->value.asBool);
	    break;
	case TypeColor:
	    v->set ((unsigned short *) sv->value.asColor.array.array);
	    break;
	case TypeString:
	    v->set (CompString (sv->value.asString));
	    break;
	case TypeMatch:
	    v->set (CompMatch (sv->value.asMatch));
	    break;
	case TypeKey:
	    {
		CompAction action;

		int keycode =
		    (sv->value.asKey.keysym == NoSymbol) ? 0 :
		    XKeysymToKeycode (screen->dpy (), sv->value.asKey.keysym);
		
		action.setKey (CompAction::KeyBinding (keycode,
			       sv->value.asKey.keyModMask));
		v->set (action);
	    }
	    break;
	case TypeButton:
	    {
		CompAction action;

		action.setEdgeMask (sv->value.asButton.edgeMask);

		action.setButton (CompAction::ButtonBinding (
				  sv->value.asButton.button,
				  sv->value.asButton.buttonModMask));
		v->set (action);
	    }
	    break;
	case TypeEdge:
	    {
		CompAction action;
		
		action.setEdgeMask (sv->value.asEdge);
		v->set (action);
	    }
	    break;
	case TypeBell:
	    {
		CompAction action;
		
		action.setBell (sv->value.asBell);

		v->set (action);
	    }
	    break;
	default:
	    break;
    }
}

static bool
ccpCCSTypeToCompizType (CCSSettingType st, CompOption::Type *ct)
{
    switch (st) {
	case TypeBool:
	    *ct = CompOption::TypeBool;
	    break;
	case TypeInt:
	    *ct = CompOption::TypeInt;
	    break;
	case TypeFloat:
	    *ct = CompOption::TypeFloat;
	    break;
	case TypeColor:
	    *ct = CompOption::TypeColor;
	    break;
	case TypeString:
	    *ct = CompOption::TypeString;
	    break;
	case TypeMatch:
	    *ct = CompOption::TypeMatch;
	    break;
	case TypeKey:
	    *ct = CompOption::TypeKey;
	    break;
	case TypeButton:
	    *ct = CompOption::TypeButton;
	    break;
	case TypeEdge:
	    *ct = CompOption::TypeEdge;
	    break;
	case TypeBell:
	    *ct = CompOption::TypeBell;
	    break;
	case TypeList:
	    *ct = CompOption::TypeList;
	    break;
	default:
	    return false;
    }

    return true;
}

static void
ccpConvertPluginList (CCSSetting          *s,
		      CCSSettingValueList list,
		      CompOption::Value   *v)
{
    CCSStringList       sl, l;
    int                 i;

    sl = ccsGetStringListFromValueList (list);

    while (ccsStringListFind(sl, (char *) "ccp"))
	sl = ccsStringListRemove (sl, (char *) "ccp", TRUE);

    while (ccsStringListFind(sl, (char *)"core"))
	sl = ccsStringListRemove (sl, (char *) "core", TRUE);

    sl = ccsStringListPrepend (sl, strdup ("ccp"));
    sl = ccsStringListPrepend (sl, strdup ("core"));

    CompOption::Value::Vector val (ccsStringListLength (sl));

    for (l = sl, i = 0; l; l = l->next)
    {
	if (l->data)
	    val[i].set (CompString (l->data));
	i++;
    }

    v->set (CompOption::TypeString, val);

    ccsStringListFree (sl, TRUE);
}

static void
ccpSettingToValue (CCSSetting        *s,
		   CompOption::Value *v)
{
    if (s->type != TypeList)
	ccpSetValueToValue (s->value, v, s->type);
    else
    {
	CCSSettingValueList list;
	int                 i = 0;
	CompOption::Type    type;

	ccsGetList (s, &list);

	if (!ccpCCSTypeToCompizType (s->info.forList.listType, &type))
	    type = CompOption::TypeBool;

	if ((strcmp (s->name, "active_plugins") == 0) &&
	    (strcmp (s->parent->name, CORE_VTABLE_NAME) == 0))
	{
	    ccpConvertPluginList (s, list, v);
	}
	else
	{
	    CompOption::Value::Vector val (ccsSettingValueListLength (list));

	    while (list)
    	    {
    		ccpSetValueToValue (list->data,
    				    &val[i],
				    s->info.forList.listType);
		list = list->next;
		i++;
	    }

	    v->set (type, val);
	}
    }
}

static void
ccpInitValue (CCSSettingValue   *value,
	      CompOption::Value *from,
	      CCSSettingType    type)
{
    switch (type)
    {
	case TypeInt:
	    value->value.asInt = from->i ();
	    break;
	case TypeFloat:
	    value->value.asFloat = from->f ();
	    break;
	case TypeBool:
	    value->value.asBool = from->b ();
	    break;
	case TypeColor:
	    {
		for (unsigned int i = 0; i < 4; i++)
		    value->value.asColor.array.array[i] = from->c ()[i];
	    }
	    break;
	case TypeString:
	    value->value.asString = strdup (from->s ().c_str ());
	    break;
	case TypeMatch:
	    value->value.asMatch = strdup (from->match ().toString ().c_str ());
	    break;
	case TypeKey:
	    if (from->action ().type () & CompAction::BindingTypeKey)
	    {
		value->value.asKey.keysym =
		    XKeycodeToKeysym (screen->dpy (),
				      from->action ().key ().keycode (), 0);
		value->value.asKey.keyModMask =
		    from->action ().key ().modifiers ();
	    }
	    else
	    {
		value->value.asKey.keysym = 0;
		value->value.asKey.keyModMask = 0;
	    }
	case TypeButton:
	    if (from->action ().type () & CompAction::BindingTypeButton)
	    {
		value->value.asButton.button =
		    from->action ().button ().button ();
		value->value.asButton.buttonModMask =
		    from->action ().button ().modifiers ();
		value->value.asButton.edgeMask = 0;
	    }
	    else if (from->action ().type () & CompAction::BindingTypeEdgeButton)
	    {
		value->value.asButton.button =
		    from->action ().button ().button ();
		value->value.asButton.buttonModMask =
		    from->action ().button ().modifiers ();
		value->value.asButton.edgeMask = from->action ().edgeMask ();
	    }
	    else
	    {
		value->value.asButton.button = 0;
		value->value.asButton.buttonModMask = 0;
		value->value.asButton.edgeMask = 0;
	    }
	    break;
	case TypeEdge:
	    value->value.asEdge = from->action ().edgeMask ();
	    break;
	case TypeBell:
	    value->value.asBell = from->action ().bell ();
	    break;
	default:
	    break;
    }
}

static void
ccpValueToSetting (CCSSetting        *s,
		   CompOption::Value *v)
{
    CCSSettingValue *value;

    value = (CCSSettingValue *) calloc (1, sizeof (CCSSettingValue));
    if (!value)
	return;

    value->parent = s;

    if (s->type == TypeList)
    {
	int i;

	foreach (CompOption::Value &lv, v->list ())
	{
	    CCSSettingValue *val;

	    val = (CCSSettingValue *) calloc (1, sizeof (CCSSettingValue));
	    if (val)
	    {
		val->parent = s;
		val->isListChild = TRUE;
		ccpInitValue (val, &lv,
			      s->info.forList.listType);
		value->value.asList =
		    ccsSettingValueListAppend (value->value.asList, val);
	    }
	}
    }
    else
	ccpInitValue (value, v, s->type);

    ccsSetValue (s, value);
    ccsFreeSettingValue (value);
}

static bool
ccpTypeCheck (CCSSetting *s, CompOption *o)
{
    CompOption::Type ot;

    switch (s->type)
    {
	case TypeList:
	    return ccpCCSTypeToCompizType (s->type, &ot) && (ot == o->type ()) &&
		   ccpCCSTypeToCompizType (s->info.forList.listType, &ot) &&
		   (ot == o->value ().listType ());
	    break;
	default:
	    return ccpCCSTypeToCompizType (s->type, &ot) && (ot == o->type ());
	    break;
    }

    return false;
}

void
CcpScreen::setOptionFromContext (CompOption *o,
				 const char *plugin)
{
    CCSPlugin         *bsp;
    CCSSetting        *setting;
    CompOption::Value value;

    int  screenNum = screen->screenNum ();

    bsp = ccsFindPlugin (mContext, (plugin) ? plugin : CORE_VTABLE_NAME);
    if (!bsp)
	return;

    setting = ccsFindSetting (bsp, o->name ().c_str (), TRUE, screenNum);
    if (!setting)
	return;

    if (!ccpTypeCheck (setting, o))
	return;

    ccpSettingToValue (setting, &value);

    mApplyingSettings = true;
    screen->setOptionForPlugin (plugin, o->name ().c_str (), value);
    mApplyingSettings = false;
}

void
CcpScreen::setContextFromOption (CompOption *o, const char *plugin)
{
    CCSPlugin       *bsp;
    CCSSetting      *setting;

    int  screenNum = screen->screenNum ();

    bsp = ccsFindPlugin (mContext, (plugin) ? plugin : CORE_VTABLE_NAME);
    if (!bsp)
	return;

    setting = ccsFindSetting (bsp, o->name ().c_str (), TRUE, screenNum);
    if (!setting)
	return;

    if (!ccpTypeCheck (setting, o))
	return;

    ccpValueToSetting (setting, &o->value ());
    ccsWriteChangedSettings (mContext);
}

bool
CcpScreen::reload ()
{
    CompPlugin *p;

    foreach (CompPlugin *p, CompPlugin::getPlugins ())
    {
	foreach (CompOption &o, p->vTable->getOptions ())
	    setOptionFromContext (&o, p->vTable->name ().c_str ());
    }

    return false;
}

bool
CcpScreen::timeout ()
{
    unsigned int flags = 0;

    if (CompPlugin::find ("glib"))
	flags |= ProcessEventsNoGlibMainLoopMask;

    ccsProcessEvents (mContext, flags);

    if (ccsSettingListLength (mContext->changedSettings))
    {
	CCSSettingList list = mContext->changedSettings;
	CCSSettingList l = list;	
	CCSSetting     *s;
    	CompPlugin     *p;
    	CompOption     *o;

	mContext->changedSettings = NULL;
	
	while (l)
	{
	    s = l->data;
	    l = l->next;

	    p = CompPlugin::find (s->parent->name);

	    if (!p)
		continue;

	    o = CompOption::findOption (p->vTable->getOptions (), s->name);
	    if (o)
		setOptionFromContext (o, s->parent->name);
	    D (D_FULL, "Setting Update \"%s\"\n", s->name);
	}

	ccsSettingListFree (list, FALSE);
	mContext->changedSettings =
	    ccsSettingListFree (mContext->changedSettings, FALSE);
    }

    return true;
}

bool
CcpScreen::setOptionForPlugin (const char *plugin,
			       const char *name,
			       CompOption::Value &v)
{
    bool status;

    status = screen->setOptionForPlugin (plugin, name, v);

    if (status && !mApplyingSettings && !mReloadTimer.active ())
    {
	CompPlugin *p;

	p = CompPlugin::find (plugin);
	if (p)
	{
	    CompOption *o;

	    o = CompOption::findOption (p->vTable->getOptions (), name);
	    if (o && (o->value () != v))
		setContextFromOption (o, p->vTable->name ().c_str ());
	}
    }

    return status;
}

bool
CcpScreen::initPluginForScreen (CompPlugin *p)
{
    bool status;

    status = screen->initPluginForScreen (p);

    if (status)
    {
	foreach (CompOption &opt, p->vTable->getOptions ())
	    setOptionFromContext (&opt, p->vTable->name ().c_str ());
    }

    return status;
}


CcpScreen::CcpScreen (CompScreen *screen) :
    PrivateHandler<CcpScreen,CompScreen> (screen),
    mApplyingSettings (false)
{
    ccsSetBasicMetadata (TRUE);

    unsigned int screenNum = screen->screenNum ();

    mContext = ccsContextNew (&screenNum, 1);
    ccsReadSettings (mContext);

    mContext->changedSettings =
	ccsSettingListFree (mContext->changedSettings, FALSE);

    mReloadTimer.start (boost::bind (&CcpScreen::reload, this), 0);
    mTimeoutTimer.start (boost::bind (&CcpScreen::timeout, this),
			 CCP_UPDATE_MIN_TIMEOUT, CCP_UPDATE_MAX_TIMEOUT);

    ScreenInterface::setHandler (screen);
}

CcpScreen::~CcpScreen ()
{
    ccsContextDestroy (mContext);
}

bool
CcpPluginVTable::init ()
{
    if (!CompPlugin::checkPluginABI ("core", CORE_ABIVERSION))
	 return false;

    getMetadata ()->addFromFile (name ());
}