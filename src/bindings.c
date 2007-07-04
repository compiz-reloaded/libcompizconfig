/*
 * Compiz configuration system library
 * 
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

/*
 * Based on Compiz option.c
 * Copyright © 2005 Novell, Inc.
 * Author: David Reveman <davidr@novell.com>
 */
 

#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

#include <X11/X.h>
#include <X11/Xlib.h>

#include <ccs.h>

#define CompAltMask        (1 << 16)
#define CompMetaMask       (1 << 17)
#define CompSuperMask      (1 << 18)
#define CompHyperMask      (1 << 19)
#define CompModeSwitchMask (1 << 20)
#define CompNumLockMask    (1 << 21)
#define CompScrollLockMask (1 << 22)

struct _Modifier {
    char *name;
    int  modifier;
} modifierList[] = {
    { "<Shift>",      ShiftMask		 },
    { "<Control>",    ControlMask	 },
    { "<Mod1>",	      Mod1Mask		 },
    { "<Mod2>",	      Mod2Mask		 },
    { "<Mod3>",	      Mod3Mask		 },
    { "<Mod4>",	      Mod4Mask		 },
    { "<Mod5>",	      Mod5Mask		 },
    { "<Alt>",	      CompAltMask        },
    { "<Meta>",	      CompMetaMask       },
    { "<Super>",      CompSuperMask      },
    { "<Hyper>",      CompHyperMask	 },
    { "<ModeSwitch>", CompModeSwitchMask },
};

#define N_MODIFIERS (sizeof (modifierList) / sizeof (struct _Modifier))

static char *edgeName[] = {
    "Left",
    "Right",
    "Top",
    "Bottom",
    "TopLeft",
    "TopRight",
    "BottomLeft",
    "BottomRight"
};

#define N_EDGES (sizeof (edgeName) / sizeof (edgeName[0]))

static char *
stringAppend (char *s,
	      char *a)
{
    char *r;
    int  len;

    len = strlen (a);

    if (s)
	len += strlen (s);

    r = malloc (len + 1);
    if (r)
    {
	if (s)
	{
	    sprintf (r, "%s%s", s, a);
	    free (s);
	}
	else
	{
	    sprintf (r, "%s", a);
	}

	s = r;
    }

    return s;
}

char *
ccsModifiersToString (unsigned int modMask)
{
    char *binding = NULL;
    int  i;

    for (i = 0; i < N_MODIFIERS; i++)
    {
	if (modMask & modifierList[i].modifier)
	    binding = stringAppend (binding, modifierList[i].name);
    }

    return binding;
}

char *
ccsKeyBindingToString (CCSSettingActionValue *action)
{
    char *binding;

    binding = ccsModifiersToString (action->keyModMask);

    if (action->keysym != NoSymbol)
    {
	char   *keyname;

	keyname = XKeysymToString (action->keysym);

	if (keyname)
	{
	    binding = stringAppend (binding, keyname);
	}
    }

    return binding;
}

char *
ccsButtonBindingToString (CCSSettingActionValue *action)
{
    char *binding;
    char buttonStr[256];

    binding = ccsModifiersToString (action->buttonModMask);

    snprintf (buttonStr, 256, "Button%d", action->button);
    binding = stringAppend (binding, buttonStr);

    return binding;
}

unsigned int
ccsStringToModifiers (const char *binding)
{
    unsigned int mods = 0;
    int		 i;

    for (i = 0; i < N_MODIFIERS; i++)
    {
	if (strcasestr (binding, modifierList[i].name))
	    mods |= modifierList[i].modifier;
    }

    return mods;
}

Bool
ccsStringToKeyBinding (const char           *binding,
		      CCSSettingActionValue *action)
{
    char	  *ptr;
    unsigned int  mods;
    KeySym	  keysym;

    mods = ccsStringToModifiers (binding);

    ptr = strrchr (binding, '>');
    if (ptr)
	binding = ptr + 1;

    while (*binding && !isalnum (*binding))
	binding++;

    keysym = XStringToKeysym (binding);
    if (keysym != NoSymbol)
    {
	action->keysym     = keysym;
	action->keyModMask = mods;

	return TRUE;
    }

    return FALSE;
}

Bool
ccsStringToButtonBinding (const char           *binding,
			 CCSSettingActionValue *action)
{
    char	 *ptr;
    unsigned int mods;

    mods = ccsStringToModifiers (binding);

    ptr = strrchr (binding, '>');
    if (ptr)
	binding = ptr + 1;

    while (*binding && !isalnum (*binding))
	binding++;

    if (strncmp (binding, "Button", strlen ("Button")) == 0)
    {
	int buttonNum;

	if (sscanf (binding + strlen ("Button"), "%d", &buttonNum) == 1)
	{
	    action->button        = buttonNum;
	    action->buttonModMask = mods;

	    return TRUE;
	}
    }

    return FALSE;
}

CCSStringList
ccsEdgeMaskToStringList (int edgeMask)
{
    CCSStringList ret = NULL;
    int i;

    for (i = 0; i < N_EDGES; i++)
	if (edgeMask & (1 << i))
	    ret = ccsStringListAppend(ret, strdup(edgeName[i]));

    return ret;
}

int
ccsStringListToEdgeMask (CCSStringList edges)
{
    int i;
    int edgeMask;
    CCSStringList l;

    edgeMask = 0;

    for (l = edges; l; l = l->next)
    {
        for (i = 0; i < N_EDGES; i++)
        {
	    if (strcmp(l->data, edgeName[i]) == 0)
	        edgeMask |= (1 << i);
        }
    }

    return edgeMask;
}

CCSStringList
ccsEdgesToStringList (CCSSettingActionValue *action)
{
    return ccsEdgeMaskToStringList(action->edgeMask);
}

void
ccsStringListToEdges (CCSStringList         edges, 
		      CCSSettingActionValue *action)
{
    action->edgeMask = ccsStringListToEdgeMask(edges);
}

CCSStringList
ccsEdgeButtonsToStringList (CCSSettingActionValue *action)
{
    return ccsEdgeMaskToStringList(action->edgeButton);
}

void
ccsStringListToEdgeButtons (CCSStringList         edgeButtons, 
			    CCSSettingActionValue *action)
{
    action->edgeButton = ccsStringListToEdgeMask(edgeButtons);
}

Bool
ccsStringToColor (const char          *value, 
		 CCSSettingColorValue *color)
{
    int c[4];

    if (sscanf (value, "#%2x%2x%2x%2x", &c[0], &c[1], &c[2], &c[3]) == 4)
    {
	color->color.red   = c[0] << 8 | c[0];
	color->color.green = c[1] << 8 | c[1];
	color->color.blue  = c[2] << 8 | c[2];
	color->color.alpha = c[3] << 8 | c[3];

	return TRUE;
    }

    return FALSE;
}

char *
ccsColorToString (CCSSettingColorValue *color)
{
    char tmp[256];

    snprintf (tmp, 256, "#%.2x%.2x%.2x%.2x",
	      color->color.red>>8, color->color.green>>8,
	      color->color.blue>>8, color->color.alpha>>8);

    return strdup (tmp);
}

