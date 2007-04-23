#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <libintl.h>
#include <dlfcn.h>
#include <dirent.h>
#include <ctype.h>
#include <libgen.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include <unistd.h>

#include <compiz.h>

#include <bsettings.h>

#include "bsettings-private.h"

static BSSettingType
getOptionType (char *name)
{
	static struct _TypeMap
	{
		char *name;
		BSSettingType type;
	} map[] =
	{
		{
		"bool", TypeBool},
		{
		"int", TypeInt},
		{
		"float", TypeFloat},
		{
		"string", TypeString},
		{
		"color", TypeColor},
		{
		"action", TypeAction},
		{
		"match", TypeMatch},
		{
		"list", TypeList}
	};
	int i;

	for (i = 0; i < sizeof (map) / sizeof (map[0]); i++)
		if (strcasecmp (name, map[i].name) == 0)
			return map[i].type;

	return CompOptionTypeBool;
}

static char *
getStringFromPath (xmlDoc * doc, xmlNode * base, char *path)
{
	xmlXPathObjectPtr xpathObj;
	xmlXPathContextPtr xpathCtx;
	char *rv = NULL;

	xpathCtx = xmlXPathNewContext (doc);
	if (base)
		xpathCtx->node = base;
	xpathObj = xmlXPathEvalExpression (BAD_CAST path, xpathCtx);
	if (!xpathObj)
		return NULL;
	xpathObj = xmlXPathConvertString (xpathObj);

	if (xpathObj->type == XPATH_STRING && xpathObj->stringval
		&& strlen ((char *)xpathObj->stringval))
	{
		rv = strdup ((char *)xpathObj->stringval);
	}

	xmlXPathFreeObject (xpathObj);
	xmlXPathFreeContext (xpathCtx);
	return rv;
}

static xmlNode **
getNodesFromPath (xmlDoc * doc, xmlNode * base, char *path, int *num)
{
	xmlXPathObjectPtr xpathObj;
	xmlXPathContextPtr xpathCtx;
	xmlNode **rv = NULL;
	int size;
	int i;

	*num = 0;

	xpathCtx = xmlXPathNewContext (doc);
	if (base)
		xpathCtx->node = base;
	xpathObj = xmlXPathEvalExpression (BAD_CAST path, xpathCtx);
	if (!xpathObj)
	{
		xmlXPathFreeContext (xpathCtx);
		return NULL;
	}

	size = (xpathObj->nodesetval) ? xpathObj->nodesetval->nodeNr : 0;

	if (!size)
	{
		xmlXPathFreeObject (xpathObj);
		xmlXPathFreeContext (xpathCtx);
		return NULL;
	}
	*num = size;

	rv = malloc (size * sizeof (xmlNode *));

	for (i = 0; i < size; i++)
		rv[i] = xpathObj->nodesetval->nodeTab[i];

	xmlXPathFreeObject (xpathObj);
	xmlXPathFreeContext (xpathCtx);
	return rv;
}

static Bool
nodeExists (xmlNode * node, char *path)
{
	xmlNode **nodes = NULL;
	int num;
	nodes = getNodesFromPath (node->doc, node, path, &num);
	if (num)
	{
		free (nodes);
		return TRUE;
	}
	return FALSE;
}

static char *
stringFromNodeDef (xmlNode * node, char *path, char *def)
{
	char *val;
	char *rv = NULL;

	val = getStringFromPath (node->doc, node, path);
	if (val)
	{
		rv = strdup (val);
		free (val);
	}
	else if (def)
		rv = strdup (def);

	return rv;
}

static char *
stringFromNodeDefTrans (xmlNode * node, char *path, char *def)
{
	char *lang;
	char newPath[1024];
	char *rv = NULL;

	lang = getenv ("LANG");
	if (!lang || !strlen (lang))
		lang = getenv ("LC_ALL");
	if (!lang || !strlen (lang))
		lang = getenv ("LC_MESSAGES");
	if (!lang || !strlen (lang))
		return stringFromNodeDef (node, path, def);

	sprintf (newPath, "%s[lang('%s')]", path, lang);
	rv = stringFromNodeDef (node, newPath, NULL);
	if (rv)
		return rv;
	sprintf (newPath, "%s[lang(substring-before('%s','.'))]", path, lang);
	rv = stringFromNodeDef (node, newPath, NULL);
	if (rv)
		return rv;
	sprintf (newPath, "%s[lang(substring-before('%s','_'))]", path, lang);
	rv = stringFromNodeDef (node, newPath, NULL);
	if (rv)
		return rv;
	sprintf (newPath, "%s[lang('C')]", path);
	rv = stringFromNodeDef (node, newPath, NULL);
	if (rv)
		return rv;
	return stringFromNodeDef (node, path, def);
}


static void
initBoolValue (BSSettingValue * v, xmlNode * node)
{
	char *value;

	v->value.asBool = FALSE;

	value = getStringFromPath (node->doc, node, "child::text()");
	if (value)
	{
		if (strcasecmp ((char *)value, "true") == 0)
			v->value.asBool = TRUE;
		free (value);
	}
}

static void
initIntValue (BSSettingValue * v, BSSettingInfo * i, xmlNode * node)
{
	char *value;

	v->value.asInt = (i->forInt.min + i->forInt.max) / 2;

	value = getStringFromPath (node->doc, node, "child::text()");
	if (value)
	{
		int val = strtol ((char *)value, NULL, 0);
		if (val >= i->forInt.min && val <= i->forInt.max)
			v->value.asInt = val;
		free (value);
	}
}

static void
initFloatValue (BSSettingValue * v, BSSettingInfo * i, xmlNode * node)
{
	char *value;

	v->value.asFloat = (i->forFloat.min + i->forFloat.max) / 2;

	setlocale (LC_NUMERIC, "C");
	value = getStringFromPath (node->doc, node, "child::text()");
	if (value)
	{
		float val = strtod ((char *)value, NULL);
		if (val >= i->forFloat.min && val <= i->forFloat.max)
			v->value.asFloat = val;
		free (value);
	}
	setlocale (LC_NUMERIC, "");
}

static void
initStringValue (BSSettingValue * v, BSSettingInfo * i, xmlNode * node)
{
	char *value;

	if (i->forString.allowedValues)
		v->value.asString = strdup (i->forString.allowedValues->data);
	else
		v->value.asString = strdup ("");

	value = getStringFromPath (node->doc, node, "child::text()");
	if (value)
	{
		if (i->forString.allowedValues)
		{
			BSStringList l = i->forString.allowedValues;
			while (l)
			{
				if (!strcmp (value, l->data))
				{
					free (v->value.asString);
					v->value.asString = strdup (value);
				}
				l = l->next;
			}
		}
		else
		{
			free (v->value.asString);
			v->value.asString = strdup (value);
		}
		free (value);
	}
}

static void
initColorValue (BSSettingValue * v, xmlNode * node)
{
	char *value;

	memset (&v->value.asColor, 0, sizeof (v->value.asColor));

	value = getStringFromPath (node->doc, node, "red/child::text()");
	if (value)
	{
		int color = strtol ((char *)value, NULL, 0);

		v->value.asColor.color.red = MAX (0, MIN (0xffff, color));
		free (value);
	}

	value = getStringFromPath (node->doc, node, "green/child::text()");
	if (value)
	{
		int color = strtol ((char *)value, NULL, 0);

		v->value.asColor.color.green = MAX (0, MIN (0xffff, color));
		free (value);
	}

	value = getStringFromPath (node->doc, node, "blue/child::text()");
	if (value)
	{
		int color = strtol ((char *)value, NULL, 0);

		v->value.asColor.color.blue = MAX (0, MIN (0xffff, color));
		free (value);
	}

	value = getStringFromPath (node->doc, node, "alpha/child::text()");
	if (value)
	{
		int color = strtol (value, NULL, 0);

		v->value.asColor.color.alpha = MAX (0, MIN (0xffff, color));
		free (value);
	}
}


static void
initMatchValue (BSSettingValue * v, xmlNode * node)
{
	char *value;

	v->value.asMatch = strdup ("");

	value = getStringFromPath (node->doc, node, "child::text()");
	if (value)
	{

		free (v->value.asMatch);
		v->value.asMatch = strdup (value);
		free (value);
	}
}

struct _Modifier
{
	char *name;
	int modifier;
} modifiers[] =
{
	{
	"<Shift>", ShiftMask},
	{
	"<Control>", ControlMask},
	{
	"<Mod1>", Mod1Mask},
	{
	"<Mod2>", Mod2Mask},
	{
	"<Mod3>", Mod3Mask},
	{
	"<Mod4>", Mod4Mask},
	{
	"<Mod5>", Mod5Mask},
	{
	"<Alt>", CompAltMask},
	{
	"<Meta>", CompMetaMask},
	{
	"<Super>", CompSuperMask},
	{
	"<Hyper>", CompHyperMask},
	{
"<ModeSwitch>", CompModeSwitchMask},};

#define N_MODIFIERS (sizeof (modifiers) / sizeof (struct _Modifier))

static unsigned int
stringToModifiers (const char *binding)
{
	unsigned int mods = 0;
	int i;

	for (i = 0; i < N_MODIFIERS; i++)
	{
		if (strcasestr (binding, modifiers[i].name))
			mods |= modifiers[i].modifier;
	}

	return mods;
}


static void
stringToKey (const char *binding, int *keysym, unsigned int *mods)
{
	char *ptr;

	*mods = stringToModifiers (binding);

	ptr = strrchr (binding, '>');
	if (ptr)
		binding = ptr + 1;

	while (*binding && !isalnum (*binding))
		binding++;

	*keysym = XStringToKeysym (binding);
}


static void
stringToButton (const char *binding, int *button, unsigned int *mods)
{
	char *ptr;

	*mods = stringToModifiers (binding);

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
			*button = buttonNum;
		}
	}
}

static void
initActionValue (BSSettingValue * v, BSSettingInfo * i, xmlNode * node)
{
	char *value;
	int j;

	memset (&v->value.asAction, 0, sizeof (v->value.asAction));

	if (i->forAction.key)
	{
		value = getStringFromPath (node->doc, node, "key/child::text()");
		if (value)
		{
			if (strcasecmp (value, "disabled"))
			{
				stringToKey (value, &v->value.asAction.keysym,
							 &v->value.asAction.keyModMask);
			}
			free (value);
		}
	}

	if (i->forAction.button)
	{
		value = getStringFromPath (node->doc, node, "button/child::text()");
		if (value)
		{
			if (strcasecmp (value, "disabled"))
			{
				stringToButton (value, &v->value.asAction.button,
								&v->value.asAction.buttonModMask);
			}
			free (value);
		}
	}

	if (i->forAction.bell)
	{
		value = getStringFromPath (node->doc, node, "bell/child::text()");
		if (value)
		{
			if (!strcasecmp (value, "true"))
			{
				v->value.asAction.onBell = TRUE;
			}
			free (value);
		}
	}

	if (i->forAction.edge)
	{
		static char *edge[] = {
			"edges/@left",
			"edges/@right",
			"edges/@top",
			"edges/@bottom",
			"edges/@top_left",
			"edges/@top_right",
			"edges/@bottom_left",
			"edges/@bottom_right"
		};

		for (j = 0; j < sizeof (edge) / sizeof (edge[0]); j++)
		{
			value = getStringFromPath (node->doc, node, edge[j]);
			if (value)
			{
				if (strcasecmp ((char *)value, "true") == 0)
					v->value.asAction.edgeMask |= (1 << j);

				free (value);
			}
		}

		value = getStringFromPath (node->doc, node, "edges/@button");
		if (value)
		{
			v->value.asAction.edgeButton = strtol ((char *)value, NULL, 0);
			free (value);
		}
	}
}

static void
initListValue (BSSettingValue * v, BSSettingInfo * i, xmlNode * node)
{
	xmlNode **nodes;
	int num, j;

	nodes = getNodesFromPath (node->doc, node, "value", &num);
	if (num)
	{
		for (j = 0; j < num; j++)
		{
			NEW (BSSettingValue, val);
			val->parent = v->parent;
			val->isListChild = TRUE;
			switch (i->forList.listType)
			{
			case TypeBool:
				initBoolValue (val, nodes[j]);
				break;
			case TypeInt:
				initIntValue (val, i->forList.listInfo, nodes[j]);
				break;
			case TypeFloat:
				initFloatValue (val, i->forList.listInfo, nodes[j]);
				break;
			case TypeString:
				initStringValue (val, i->forList.listInfo, nodes[j]);
				break;
			case TypeColor:
				initColorValue (val, nodes[j]);
				break;
			case TypeAction:
				initActionValue (val, i->forList.listInfo, nodes[j]);
				break;
			case TypeMatch:
				initMatchValue (val, nodes[j]);
			default:
				break;
			}
			v->value.asList = bsSettingValueListAppend (v->value.asList, val);
		}
		free (nodes);
	}
}

static void
initIntInfo (BSSettingInfo * i, xmlNode * node)
{
	char *value;
	i->forInt.min = MINSHORT;
	i->forInt.max = MAXSHORT;

	value = getStringFromPath (node->doc, node, "min/child::text()");
	if (value)
	{
		int val = strtol (value, NULL, 0);
		i->forInt.min = val;
		free (value);
	}
	value = getStringFromPath (node->doc, node, "max/child::text()");
	if (value)
	{
		int val = strtol (value, NULL, 0);
		i->forInt.max = val;
		free (value);
	}
}

static void
initFloatInfo (BSSettingInfo * i, xmlNode * node)
{
	char *value;
	i->forFloat.min = MINSHORT;
	i->forFloat.max = MAXSHORT;
	i->forFloat.precision = 0.1f;
	setlocale (LC_NUMERIC, "C");
	value = getStringFromPath (node->doc, node, "min/child::text()");
	if (value)
	{
		float val = strtod (value, NULL);
		i->forFloat.min = val;
		free (value);
	}
	value = getStringFromPath (node->doc, node, "max/child::text()");
	if (value)
	{
		float val = strtod (value, NULL);
		i->forFloat.max = val;
		free (value);
	}
	value = getStringFromPath (node->doc, node, "precision/child::text()");
	if (value)
	{
		float val = strtod (value, NULL);
		i->forFloat.precision = val;
		free (value);
	}
	setlocale (LC_NUMERIC, "");
}

static void
initStringInfo (BSSettingInfo * i, xmlNode * node)
{
	xmlNode **nodes;
	char *value;
	int num, j;

	i->forString.allowedValues = NULL;

	nodes = getNodesFromPath (node->doc, node, "allowed/value", &num);
	if (num)
	{
		for (j = 0; j < num; j++)
		{
			value = getStringFromPath (node->doc, nodes[j], "child::text()");
			if (value)
			{
				char *string = strdup (value);
				i->forString.allowedValues =
					bsStringListAppend (i->forString.allowedValues, string);
				free (value);
			}
		}
		free (nodes);
	}
}

static void
initActionInfo (BSSettingInfo * i, xmlNode * node)
{
	char *value;
	i->forAction.key = FALSE;
	i->forAction.button = FALSE;
	i->forAction.edge = FALSE;
	i->forAction.bell = FALSE;


	value = getStringFromPath (node->doc, node, "allowed/@key");
	if (value)
	{
		if (!strcasecmp (value, "true"))
			i->forAction.key = TRUE;
		free (value);
	}
	value = getStringFromPath (node->doc, node, "allowed/@button");
	if (value)
	{
		if (!strcasecmp (value, "true"))
			i->forAction.button = TRUE;
		free (value);
	}
	value = getStringFromPath (node->doc, node, "allowed/@bell");
	if (value)
	{
		if (!strcasecmp (value, "true"))
			i->forAction.bell = TRUE;
		free (value);
	}
	value = getStringFromPath (node->doc, node, "allowed/@edge");
	if (value)
	{
		if (!strcasecmp (value, "true"))
			i->forAction.edge = TRUE;
		free (value);
	}
	value = getStringFromPath (node->doc, node, "allowed/@edgednd");
	if (value)
	{
		if (!strcasecmp (value, "true"))
			i->forAction.edge = TRUE;
		free (value);
	}

}


static void
initListInfo (BSSettingInfo * i, xmlNode * node)
{
	char *value;

	i->forList.listType = TypeBool;
	i->forList.listInfo = NULL;

	value = getStringFromPath (node->doc, node, "type/child::text()");
	if (!value)
		return;
	i->forList.listType = getOptionType (value);
	free (value);
	switch (i->forList.listType)
	{
	case TypeInt:
	{
		NEW (BSSettingInfo, info);
		initIntInfo (info, node);
		i->forList.listInfo = info;
	}
		break;
	case TypeFloat:
	{
		NEW (BSSettingInfo, info);
		initFloatInfo (info, node);
		i->forList.listInfo = info;
	}
		break;
	case TypeString:
	{
		NEW (BSSettingInfo, info);
		initStringInfo (info, node);
		i->forList.listInfo = info;
	}
		break;
	case TypeAction:
	{
		NEW (BSSettingInfo, info);
		initActionInfo (info, node);
		i->forList.listInfo = info;
	}
		break;
	default:
		break;
	}

}

static void
printSetting (BSSetting * s)
{
	char *val;
	printf ("Name        : %s\n", s->name);
	printf ("Short       : %s\n", s->shortDesc);
	printf ("Long        : %s\n", s->longDesc);

	if (s->group && strlen (s->group))
		printf ("Group       : %s\n", s->group);
	if (s->subGroup && strlen (s->subGroup))
		printf ("Subgroup    : %s\n", s->subGroup);
	if (s->hints && strlen (s->hints))
		printf ("Hints       : %s\n", s->hints);
	switch (s->type)
	{
	case TypeInt:
		printf ("Type        : int\n");
		printf ("Value       : %d (Min : %d / Max : %d)\n",
				s->value->value.asInt, s->info.forInt.min,
				s->info.forInt.max);
		break;
	case TypeBool:
		printf ("Type        : bool\n");
		printf ("Value       : %s \n",
				(s->value->value.asBool) ? "true" : "false");
		break;
	case TypeFloat:
		printf ("Type        : float\n");
		printf ("Value       : %g (Min : %g / Max : %g / Prec : %g)\n",
				s->value->value.asFloat, s->info.forFloat.min,
				s->info.forFloat.max, s->info.forFloat.precision);
		break;
	case TypeString:
	{
		printf ("Type        : string\n");
		printf ("Value       : %s\n", s->value->value.asString);
		BSStringList l = s->info.forString.allowedValues;
		if (l)
		{
			printf ("   Allowed  : ");
			while (l)
			{
				printf ("%s,", l->data);
				l = l->next;
			}
			printf ("\n");
		}
	}
		break;
	case TypeColor:
		printf ("Type        : color\n");
		printf
			("Value       : red : 0x%x green : 0x%x blue : 0x%x alpha : 0x%x\n",
			 s->value->value.asColor.color.red,
			 s->value->value.asColor.color.green,
			 s->value->value.asColor.color.blue,
			 s->value->value.asColor.color.alpha);
		break;
	case TypeAction:
		printf ("Type        : action\n");
		if (s->info.forAction.key)
		{
			val = bsKeyBindingToString (&s->value->value.asAction);
			printf ("    Key     : %s\n", val);
			if (val)
				free (val);
		}
		if (s->info.forAction.button)
		{
			val = bsButtonBindingToString (&s->value->value.asAction);
			printf ("    Button  : %s\n", val);
			if (val)
				free (val);
		}
		if (s->info.forAction.edge)
		{
			val = bsEdgeToString (&s->value->value.asAction);
			printf ("    Edge    : %s\n", val);
			if (val)
				free (val);
		}
		if (s->info.forAction.bell)
			printf ("    Bell    : %s\n",
					(s->value->value.asAction.onBell) ? "true" : "false");
		break;
	case TypeMatch:
		printf ("Type        : match\n");
		printf ("Value       : %s\n", s->value->value.asMatch);
		break;
	case TypeList:
	{
		switch (s->info.forList.listType)
		{
		case TypeInt:
			printf ("Type        : list (int)\n");
			printf ("    Info    : Min : %d Max : %d\n",
					s->info.forList.listInfo->forInt.min,
					s->info.forList.listInfo->forInt.max);
			break;
		case TypeBool:
			printf ("Type        : list (bool)\n");
			break;
		case TypeFloat:
			printf ("Type        : list (float)\n");
			printf ("    Info    : Min : %g Max : %g Prec : %g\n",
					s->info.forList.listInfo->forFloat.min,
					s->info.forList.listInfo->forFloat.max,
					s->info.forList.listInfo->forFloat.precision);
			break;
		case TypeString:
		{
			printf ("Type        : list (string)\n");
			BSStringList l =
				s->info.forList.listInfo->forString.allowedValues;
			if (l)
			{
				printf ("   Allowed  : ");
				while (l)
				{
					printf ("%s,", l->data);
					l = l->next;
				}
				printf ("\n");
			}
		}
			break;
		case TypeColor:
			printf ("Type        : list (color)\n");
			break;
		case TypeAction:
			printf ("Type        : list (action)\n");
			break;
		case TypeMatch:
			printf ("Type        : list (match)\n");
			break;
		default:
			break;
		}
		BSSettingValueList l = s->value->value.asList;
		if (!l)
			break;
		printf ("Values      : ");
		while (l)
		{
			BSSettingValue *val = l->data;
			switch (s->info.forList.listType)
			{
			case TypeInt:
				printf ("%d,", val->value.asInt);
				break;
			case TypeBool:
				printf ("%s,", (val->value.asBool) ? "true" : "false");
				break;
			case TypeFloat:
				printf ("%g,", val->value.asFloat);
				break;
			case TypeString:
				printf ("%s,", val->value.asString);
				break;
			case TypeColor:
			{
				char *str = bsColorToString (&val->value.asColor);
				printf ("%s,", str);
				if (str)
					free (str);
			}
				break;
			case TypeMatch:
				printf ("%s,", val->value.asMatch);
				break;
			default:
				break;
			}
			l = l->next;
		}
		printf ("\n");
	}
		break;
	default:
		break;
	}
	printf ("\n");
}

static void
addOptionFromXMLNode (BSPlugin * plugin, xmlNode * node)
{
	char *name;
	char *type;
	xmlNode **nodes;
	int num;
	Bool screen;

	if (!node)
		return;

	name = getStringFromPath (node->doc, node, "@name");
	type = getStringFromPath (node->doc, node, "@type");
	if (!name || !strlen (name) || !type || !strlen (type))
	{
		if (name)
			free (name);
		if (type)
			free (type);
		return;
	}

	screen = nodeExists (node, "ancestor::screen");

	if (bsFindSetting (plugin, name, screen, 0))
	{
		fprintf (stderr, "[ERROR]: Option \"%s\" already defined\n", name);
		free (name);
		free (type);
		return;
	}

	NEW (BSSetting, setting);

	setting->parent = plugin;
	setting->isScreen = screen;
	setting->screenNum = 0;
	setting->isDefault = TRUE;
	setting->name = strdup (name);
	setting->shortDesc =
		stringFromNodeDefTrans (node, "short/child::text()", name);
	setting->longDesc =
		stringFromNodeDefTrans (node, "long/child::text()", "");
	setting->hints = stringFromNodeDef (node, "hints/child::text()", "");
	setting->group =
		stringFromNodeDefTrans (node, "ancestor::group/short/child::text()",
								"");
	setting->subGroup =
		stringFromNodeDefTrans (node,
								"ancestor::subgroup/short/child::text()", "");
	setting->type = getOptionType (type);
	setting->value = &setting->defaultValue;
	setting->defaultValue.parent = setting;
	free (name);
	free (type);

	switch (setting->type)
	{
	case TypeInt:
		initIntInfo (&setting->info, node);
		break;
	case TypeFloat:
		initFloatInfo (&setting->info, node);
		break;
	case TypeString:
		initStringInfo (&setting->info, node);
		break;
	case TypeAction:
		initActionInfo (&setting->info, node);
		break;
	case TypeList:
		initListInfo (&setting->info, node);
		break;
	default:
		break;
	}

	nodes = getNodesFromPath (node->doc, node, "default", &num);

	if (num)
	{
		switch (setting->type)
		{
		case TypeInt:
			initIntValue (&setting->defaultValue, &setting->info, nodes[0]);
			break;
		case TypeBool:
			initBoolValue (&setting->defaultValue, nodes[0]);
			break;
		case TypeFloat:
			initFloatValue (&setting->defaultValue, &setting->info, nodes[0]);
			break;
		case TypeString:
			initStringValue (&setting->defaultValue, &setting->info,
							 nodes[0]);
			break;
		case TypeColor:
			initColorValue (&setting->defaultValue, nodes[0]);
			break;
		case TypeAction:
			initActionValue (&setting->defaultValue, &setting->info,
							 nodes[0]);
			break;
		case TypeMatch:
			initMatchValue (&setting->defaultValue, nodes[0]);
			break;
		case TypeList:
			initListValue (&setting->defaultValue, &setting->info, nodes[0]);
			break;
		default:
			break;
		}
		free (nodes);
	}

	printSetting (setting);
	plugin->settings = bsSettingListAppend (plugin->settings, setting);
}

static void
initOptionsFromRootNode (BSPlugin * plugin, xmlNode * node)
{
	xmlNode **nodes;
	int num, i;
	nodes = getNodesFromPath (node->doc, node, ".//option", &num);
	if (num)
	{
		for (i = 0; i < num; i++)
			addOptionFromXMLNode (plugin, nodes[i]);
		free (nodes);
	}
}

static void
initRuleFromNode (BSPlugin * plugin, xmlNode * node)
{
	char *type = stringFromNodeDef (node, "@type", "");
	char *rule = stringFromNodeDef (node, "child::text()", NULL);
	if (!rule || !strlen (rule))
	{
		if (rule)
			free (rule);
		free (type);
		return;
	}
	if (!strcmp (type, "before"))
	{
		plugin->loadBefore = bsStringListAppend (plugin->loadBefore, rule);
	}
	else if (!strcmp (type, "after"))
	{
		plugin->loadAfter = bsStringListAppend (plugin->loadAfter, rule);
	}
	else if (!strcmp (type, "require"))
	{
		plugin->requiresPlugin =
			bsStringListAppend (plugin->requiresPlugin, rule);
	}
	else if (!strcmp (type, "require_feature"))
	{
		plugin->requiresFeature =
			bsStringListAppend (plugin->requiresFeature, rule);
	}
	free (type);
}

static void
initFeatureFromNode (BSPlugin * plugin, xmlNode * node)
{
	char *feature = stringFromNodeDef (node, "child::text()", NULL);
	if (feature && strlen (feature))
		plugin->providesFeature = bsStringListAppend (plugin->providesFeature,
													  feature);
	if (feature && !strlen (feature))
		free (feature);
}


static void
initRulesFromRootNode (BSPlugin * plugin, xmlNode * node)
{
	xmlNode **nodes;
	int num, i;
	nodes = getNodesFromPath (node->doc, node, "rule", &num);
	if (num)
	{
		for (i = 0; i < num; i++)
			initRuleFromNode (plugin, nodes[i]);
		free (nodes);
	}
	nodes = getNodesFromPath (node->doc, node, "feature", &num);
	if (num)
	{
		for (i = 0; i < num; i++)
			initFeatureFromNode (plugin, nodes[i]);
		free (nodes);
	}
}

static void
addPluginFromXMLNode (BSContext * context, xmlNode * node)
{
	char *name;

	if (!node)
		return;

	name = getStringFromPath (node->doc, node, "@name");
	if (!name || !strlen (name))
	{
		if (name)
			free (name);
		return;
	}

	if (bsFindPlugin (context, name))
		return;

	if (!strcmp(name,"ini") || !strcmp(name,"gconf") || !strcmp(name,"bset"))
	    return;

	
	NEW (BSPlugin, plugin);

	plugin->context = context;
	plugin->name = strdup (name);

	plugin->shortDesc =
		stringFromNodeDefTrans (node, "short/child::text()", name);
	plugin->longDesc =
		stringFromNodeDefTrans (node, "long/child::text()", name);

	initRulesFromRootNode (plugin, node);

	plugin->category = stringFromNodeDef (node, "category/child::text()", "");

	printf ("Adding plugin %s (%s)\n", name, plugin->shortDesc);
//TODO: Rules and features

	initOptionsFromRootNode (plugin, node);

	if (!bsFindSetting (plugin, "____plugin_enabled", FALSE, 0))
	{
		NEW (BSSetting, setting);

		setting->parent = plugin;
		setting->isScreen = FALSE;
		setting->screenNum = 0;
		setting->isDefault = TRUE;
		setting->name = strdup ("____plugin_enabled");
		setting->shortDesc = strdup ("enabled");
		setting->longDesc = strdup ("enabled");
		setting->group = strdup ("");
		setting->subGroup = strdup ("");
		setting->type = TypeBool;
		setting->defaultValue.parent = setting;
		setting->defaultValue.value.asBool = FALSE;

		setting->value = &setting->defaultValue;

		plugin->settings = bsSettingListAppend (plugin->settings, setting);
	}
	else
	{
		fprintf (stderr, "Couldn't load plugin %s because it has a setting "
				 "defined named ____plugin_enabled", plugin->name);
	}

	collateGroups (plugin);
	context->plugins = bsPluginListAppend (context->plugins, plugin);
	free (name);
}

static void
addCoreSettingsFromXMLNode (BSContext * context, xmlNode * node)
{
	if (!node)
		return;

	if (bsFindPlugin (context, "core"))
		return;

	NEW (BSPlugin, plugin);

	plugin->context = context;
	plugin->name = strdup ("core");
	plugin->category = strdup ("General");


	plugin->shortDesc =
		stringFromNodeDefTrans (node, "short/child::text()",
								"General Options");
	plugin->longDesc =
		stringFromNodeDefTrans (node, "long/child::text()",
								"General Compiz Options");

	printf ("Adding core settings (%s)\n", plugin->shortDesc);

	initOptionsFromRootNode (plugin, node);

	collateGroups (plugin);
	context->plugins = bsPluginListAppend (context->plugins, plugin);
}

static void
loadPluginsFromXML (BSContext * context, xmlDoc * doc)
{
	xmlNode **nodes;
	int num, i;
	nodes = getNodesFromPath (doc, NULL, "/compiz/core", &num);
	if (num)
	{
		addCoreSettingsFromXMLNode (context, nodes[0]);
		free (nodes);
	}
	nodes = getNodesFromPath (doc, NULL, "/compiz/plugin", &num);
	if (num)
	{
		for (i = 0; i < num; i++)
			addPluginFromXMLNode (context, nodes[i]);
		free (nodes);
	}
}

static int
pluginNameFilter (const struct dirent *name)
{
	int length = strlen (name->d_name);

	if (length < 7)
		return 0;

	if (strncmp (name->d_name, "lib", 3) ||
		strncmp (name->d_name + length - 3, ".so", 3))
		return 0;

	return 1;
}

static int
pluginXMLFilter (const struct dirent *name)
{
	int length = strlen (name->d_name);

	if (length < 5)
		return 0;

	if (strncmp (name->d_name + length - 4, ".xml", 4))
		return 0;

	return 1;
}

static void
loadPluginsFromXMLFiles (BSContext * context, char *path)
{
	struct dirent **nameList;
	char *name;
	int nFile, i;
	FILE *fp;
	xmlDoc *doc = NULL;

	if (!path)
		return;

	nFile = scandir (path, &nameList, pluginXMLFilter, NULL);

	if (nFile <= 0)
		return;

	for (i = 0; i < nFile; i++)
	{
		asprintf (&name, "%s/%s", path, nameList[i]->d_name);
		free (nameList[i]);

		fp = fopen (name, "r");
		if (fp)
		{
			fclose (fp);
			doc = xmlReadFile (name, NULL, 0);
			if (doc)
				loadPluginsFromXML (context, doc);
			xmlFreeDoc (doc);
		}
		free (name);
	}
	free (nameList);

}

static void
addPluginNamed (BSContext * context, char *name)
{

	if (bsFindPlugin (context, name))
		return;
	if (!strcmp(name,"ini") || !strcmp(name,"gconf") || !strcmp(name,"bset"))
	    return;

	NEW (BSPlugin, plugin);

	plugin->context = context;
	plugin->name = strdup (name);

	printf ("Adding plugin named %s\n", name);

	if (!plugin->shortDesc)
		plugin->shortDesc = strdup (name);
	if (!plugin->longDesc)
		plugin->longDesc = strdup (name);

	if (plugin->category)
		plugin->category = strdup ("");

	NEW (BSSetting, setting);

	setting->parent = plugin;
	setting->isScreen = FALSE;
	setting->screenNum = 0;
	setting->isDefault = TRUE;
	setting->name = strdup ("____plugin_enabled");
	setting->shortDesc = strdup ("enabled");
	setting->longDesc = strdup ("enabled");
	setting->group = strdup ("");
	setting->subGroup = strdup ("");
	setting->type = TypeBool;
	setting->defaultValue.parent = setting;
	setting->defaultValue.value.asBool = FALSE;

	setting->value = &setting->defaultValue;

	plugin->settings = bsSettingListAppend (plugin->settings, setting);

	collateGroups (plugin);
	context->plugins = bsPluginListAppend (context->plugins, plugin);
}

static void
loadPluginsFromName (BSContext * context, char *path)
{
	struct dirent **nameList;
	int nFile, i;

	if (!path)
		return;

	nFile = scandir (path, &nameList, pluginNameFilter, NULL);

	if (nFile <= 0)
		return;

	for (i = 0; i < nFile; i++)
	{
		char name[1024];

		sscanf (nameList[i]->d_name, "lib%s", name);
		if (strlen (name) > 3)
			name[strlen (name) - 3] = 0;
		free (nameList[i]);

		addPluginNamed (context, name);
	}
	free (nameList);

}

void
bsLoadPlugins (BSContext * context)
{
	char *home = getenv ("HOME");
	if (home && strlen (home))
	{
		char *homeplugins = NULL;
		asprintf (&homeplugins, "%s/.compiz/metadata", home);
		loadPluginsFromXMLFiles (context, homeplugins);
		free (homeplugins);
	}
	loadPluginsFromXMLFiles (context, METADATADIR);

	if (home && strlen (home))
	{
		char *homeplugins = NULL;
		asprintf (&homeplugins, "%s/.compiz/plugins", home);
		loadPluginsFromName (context, homeplugins);
		free (homeplugins);
	}
	loadPluginsFromName (context, PLUGINDIR);

}
