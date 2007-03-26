/*
 * Copyright © 2005 Novell, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Novell, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * Novell, Inc. makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * NOVELL, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL NOVELL, INC. BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: David Reveman <davidr@novell.com>
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <math.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <bsettings.h>

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
} modifiers[] = {
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
	{ "<Hyper>",      CompHyperMask	     },
    { "<ModeSwitch>", CompModeSwitchMask },
};

#define N_MODIFIERS (sizeof (modifiers) / sizeof (struct _Modifier))

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

#define N_EDGENAMES (sizeof (edgeName) / sizeof (edgeName[0]))

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

static char *
modifiersToString (unsigned int modMask)
{
    char *binding = NULL;
    int  i;

    for (i = 0; i < N_MODIFIERS; i++)
    {
		if (modMask & modifiers[i].modifier)
			binding = stringAppend (binding, modifiers[i].name);
	}

    return binding;
}

char *
bsKeyBindingToString (BSSettingActionValue *action)
{
	char *binding;

	binding = modifiersToString (action->keyModMask);

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
bsButtonBindingToString (BSSettingActionValue *action)
{
    char *binding;
    char buttonStr[256];

    binding = modifiersToString (action->buttonModMask);

    snprintf (buttonStr, 256, "Button%d", action->button);
    binding = stringAppend (binding, buttonStr);

    return binding;
}

static unsigned int
stringToModifiers (const char *binding)
{
    unsigned int mods = 0;
    int		 i;

    for (i = 0; i < N_MODIFIERS; i++)
    {
		if (strcasestr (binding, modifiers[i].name))
			mods |= modifiers[i].modifier;
	}

    return mods;
}

Bool
bsStringToKeyBinding (const char           *binding,
  					  BSSettingActionValue *action)
{
    char	  *ptr;
    unsigned int  mods;
    KeySym	  keysym;

    mods = stringToModifiers (binding);

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
bsStringToButtonBinding (const char           *binding,
		  				 BSSettingActionValue *action)
{
    char	 *ptr;
    unsigned int mods;

    mods = stringToModifiers (binding);

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

char *
bsEdgeToString (BSSettingActionValue *action)
{
	int i;

	for (i = 0; i < N_EDGENAMES; i++)
	{
		if (action->edgeMask & (1 << i))
			return strdup (edgeName[i]);
	}

	return strdup ("");
}

void bsStringToEdge (const char           *edge,
		  			 BSSettingActionValue *action)
{
    int i;

    action->edgeMask = 0;

    for (i = 0; i < N_EDGENAMES; i++)
	{
		if (strcmp(edge, edgeName[i]) == 0)
		{
			action->edgeMask = (1 << i);
			break;
		}
	}
}

Bool
bsStringToColor (const char          *value,
				 BSSettingColorValue *color)
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
bsColorToString (BSSettingColorValue *color)
{
    char tmp[256];

    snprintf (tmp, 256, "#%.2x%.2x%.2x%.2x",
	      color->color.red / 256, 
		  color->color.green / 256, 
		  color->color.blue / 256, 
		  color->color.alpha / 256);

    return strdup (tmp);
}

