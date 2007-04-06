#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <libintl.h>
#include <dlfcn.h>
#include <ctype.h>
#include <libgen.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <unistd.h>


#include <compiz.h>

#include <bsettings.h>

#include "bsettings-private.h"

typedef struct _ParserState {
	Bool screen;
	char *group;
	char *subGroup;
} ParserState;

typedef enum {
	OptionTypeInt,
	OptionTypeBool,
	OptionTypeFloat,
	OptionTypeString,
	OptionTypeStringList,
	OptionTypeEnum,
	OptionTypeSelection,
	OptionTypeColor,
	OptionTypeAction,
	OptionTypeMatch,
	OptionTypeError
} OptionType;

static OptionType getOptionType(xmlChar * type)
{
	if (!xmlStrcmp(type, (const xmlChar *) "int"))
		return OptionTypeInt;
	if (!xmlStrcmp(type, (const xmlChar *) "bool"))
		return OptionTypeBool;
	if (!xmlStrcmp(type, (const xmlChar *) "float"))
		return OptionTypeFloat;
	if (!xmlStrcmp(type, (const xmlChar *) "string"))
		return OptionTypeString;
	if (!xmlStrcmp(type, (const xmlChar *) "stringlist"))
		return OptionTypeStringList;
	if (!xmlStrcmp(type, (const xmlChar *) "enum"))
		return OptionTypeEnum;
	if (!xmlStrcmp(type, (const xmlChar *) "selection"))
		return OptionTypeSelection;
	if (!xmlStrcmp(type, (const xmlChar *) "color"))
		return OptionTypeColor;
	if (!xmlStrcmp(type, (const xmlChar *) "action"))
		return OptionTypeAction;
	if (!xmlStrcmp(type, (const xmlChar *) "match"))
		return OptionTypeMatch;
	return OptionTypeError;
}

static int nameCheck(char * str)
{
	if (!str)
		return 0;
	if (!strlen(str))
		return 0;
	int i = 0;
	for (i = 0; i < strlen(str);i++)
		if (isspace(str[i]) || !isprint(str[i]))
			return 0;
	return 1;
}

static void parseElements(BSPlugin *p, xmlDoc *doc, ParserState* pState, xmlNode *node);

static void
parseIntOption(BSSetting *s, xmlDoc *doc, xmlNode *node)
{
	xmlNode *cur = node;
	while (cur != NULL)
	{
		if (!xmlStrcmp(cur->name, (const xmlChar *) "min")) {
			xmlChar *key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			s->info.forInt.min = strtol((char *)key,NULL,0);
			xmlFree(key);
		} else if (!xmlStrcmp(cur->name, (const xmlChar *) "max")) {
			xmlChar *key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			s->info.forInt.max = strtol((char *)key,NULL,0);
			xmlFree(key);
		} else if (!xmlStrcmp(cur->name, (const xmlChar *) "default")) {
			xmlChar *key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			s->defaultValue.value.asInt = strtol((char *)key,NULL,0);
			xmlFree(key);
		}
		cur = cur->next;
	}
}

static void
parseBoolOption(BSSetting *s, xmlDoc *doc, xmlNode *node)
{
	xmlNode *cur = node;
	while (cur != NULL)
	{
		if (!xmlStrcmp(cur->name, (const xmlChar *) "default")) {
			xmlChar *key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			s->defaultValue.value.asBool = (strcasecmp((char *)key,"true") == 0);
			xmlFree(key);
		}
		cur = cur->next;
	}
}

static void
parseFloatOption(BSSetting *s, xmlDoc *doc, xmlNode *node)
{
	xmlNode *cur = node;
	while (cur != NULL)
	{
		if (!xmlStrcmp(cur->name, (const xmlChar *) "min")) {
			xmlChar *key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			s->info.forFloat.min = strtod((char *)key,NULL);
			xmlFree(key);
		} else if (!xmlStrcmp(cur->name, (const xmlChar *) "max")) {
			xmlChar *key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			s->info.forFloat.max = strtod((char *)key,NULL);
			xmlFree(key);
		} else if (!xmlStrcmp(cur->name, (const xmlChar *) "precision")) {
			xmlChar *key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			s->info.forFloat.precision = strtod((char *)key,NULL);
			xmlFree(key);
		} else if (!xmlStrcmp(cur->name, (const xmlChar *) "default")) {
			xmlChar *key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			s->defaultValue.value.asFloat = strtod((char *)key,NULL);
			xmlFree(key);
		}
		cur = cur->next;
	}
}

static void
parseStringOption(BSSetting *s, xmlDoc *doc, xmlNode *node)
{
	xmlNode *cur = node;
	while (cur != NULL)
	{
		if (!xmlStrcmp(cur->name, (const xmlChar *) "default")) {
			xmlChar *key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			s->defaultValue.value.asString = strdup((key)?(char *)key:"");
			xmlFree(key);
		}
		cur = cur->next;
	}
}

static void
parseMatchOption(BSSetting *s, xmlDoc *doc, xmlNode *node)
{
	xmlNode *cur = node;
	while (cur != NULL)
	{
		if (!xmlStrcmp(cur->name, (const xmlChar *) "default")) {
			xmlChar *key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			s->defaultValue.value.asMatch = strdup((key)?(char *)key:"");
			xmlFree(key);
		}
		cur = cur->next;
	}
}

static void
parseColorOption(BSSetting *s, xmlDoc *doc, xmlNode *node)
{
	xmlNode *cur = node;
	while (cur != NULL)
	{
		if (!xmlStrcmp(cur->name, (const xmlChar *) "red")) {
			xmlChar *key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			s->defaultValue.value.asColor.color.red =
					MAX(0,MIN(0xffff,strtol((char *)key,NULL,0)));
			xmlFree(key);
		} else if (!xmlStrcmp(cur->name, (const xmlChar *) "green")) {
			xmlChar *key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			s->defaultValue.value.asColor.color.green =
					MAX(0,MIN(0xffff,strtol((char *)key,NULL,0)));
			xmlFree(key);
		} else if (!xmlStrcmp(cur->name, (const xmlChar *) "blue")) {
			xmlChar *key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			s->defaultValue.value.asColor.color.blue =
					MAX(0,MIN(0xffff,strtol((char *)key,NULL,0)));
			xmlFree(key);
		} else if (!xmlStrcmp(cur->name, (const xmlChar *) "alpha")) {
			xmlChar *key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			s->defaultValue.value.asColor.color.alpha =
					MAX(0,MIN(0xffff,strtol((char *)key,NULL,0)));
			xmlFree(key);
		}
		cur = cur->next;
	}
}

static void
parseStringListOption(BSSetting *s, xmlDoc *doc, xmlNode *node)
{
	NEW(BSSettingInfo, listInfo);
	s->info.forList.listType = TypeList;
	s->info.forList.listInfo = listInfo;

	xmlNode *cur = node;
	while (cur != NULL)
	{
		if (!xmlStrcmp(cur->name, (const xmlChar *) "default")) {
			xmlChar *key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key && strlen((char *)key))
			{
				NEW(BSSettingValue, val);
				val->parent = s;
				val->isListChild = TRUE;
				val->value.asString = strdup((char *)key);
				s->defaultValue.value.asList = bsSettingValueListAppend(
						s->defaultValue.value.asList, val);
			}
			xmlFree(key);
		}
		cur = cur->next;
	}
}

static void
parseEnumOption(BSSetting *s, xmlDoc *doc, xmlNode *node)
{
	xmlNode *cur = node;
	while (cur != NULL)
	{
		if (!xmlStrcmp(cur->name, (const xmlChar *) "value")) {
			xmlChar *key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (!key || !strlen((char *)key))
				key = xmlGetProp(cur, (xmlChar *)"name");
			if (key && strlen((char *)key))
			{
				s->info.forString.allowedValues =
						bsStringListAppend(s->info.forString.allowedValues, (char *)key);
				xmlChar *def;
				def = xmlGetProp(cur, (xmlChar *)"default");
				if (def && strcasecmp((char *)def,"true") == 0)
					if (!s->defaultValue.value.asString)
						s->defaultValue.value.asString = strdup((char *)key);
				xmlFree(def);
			}
			xmlFree(key);
		}
		cur = cur->next;
	}
}

static void
parseSelectionOption(BSSetting *s, xmlDoc *doc, xmlNode *node)
{
	NEW(BSSettingInfo, listInfo);
	s->info.forList.listType = TypeList;
	s->info.forList.listInfo = listInfo;

	xmlNode *cur = node;
	while (cur != NULL)
	{
		if (!xmlStrcmp(cur->name, (const xmlChar *) "value")) {
			xmlChar *key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (!key || !strlen((char *)key))
				key = xmlGetProp(cur, (xmlChar *)"name");
			if (key && strlen((char *)key))
			{
				listInfo->forString.allowedValues =
						bsStringListAppend(listInfo->forString.allowedValues, (char *)key);
				xmlChar *def;
				def = xmlGetProp(cur, (xmlChar *)"default");
				if (def && strcasecmp((char *)def,"true") == 0)
				{
					NEW(BSSettingValue, val);
					val->parent = s;
					val->isListChild = TRUE;
					val->value.asString = strdup((char *)key);
					s->defaultValue.value.asList = bsSettingValueListAppend(
							s->defaultValue.value.asList, val);
				}
				xmlFree(def);
			}
			xmlFree(key);
		}
		cur = cur->next;
	}
}

struct _Modifier {
    char *name;
    int  modifier;
} modifiers[] = {
    { "Shift",      ShiftMask		 },
    { "Control",    ControlMask	 },
    { "Mod1",	      Mod1Mask		 },
    { "Mod2",	      Mod2Mask		 },
    { "Mod3",	      Mod3Mask		 },
    { "Mod4",	      Mod4Mask		 },
    { "Mod5",	      Mod5Mask		 },
    { "Alt",	      CompAltMask        },
    { "Meta",	      CompMetaMask       },
    { "Super",      CompSuperMask      },
    { "Hyper",      CompHyperMask	 },
    { "ModeSwitch", CompModeSwitchMask },
};

#define N_MODIFIERS (sizeof (modifiers) / sizeof (struct _Modifier))

static void convertKey(char *bind, unsigned int *mod, int *keysym)
{
	char *tok = strtok(bind,"+");
	int i;
	*keysym = 0;
	*mod = 0;
	Bool changed = FALSE;

	while (tok)
	{
		changed = FALSE;
		for (i = 0; i < N_MODIFIERS && !changed; i++)
			if (!strcmp(tok,modifiers[i].name))
			{
				*mod |= modifiers[i].modifier;
				changed = TRUE;
			}
		if (!changed && !keysym)
			*keysym = XStringToKeysym(tok);

		tok = strtok(NULL,"+");
	}
}

static void convertButton(char *bind, unsigned int *mod, int *button)
{
	char *tok = strtok(bind,"+");
	int i;
	*button = 0;
	*mod = 0;
	Bool changed;

	while (tok)
	{
		changed = FALSE;
		for (i = 0; i < N_MODIFIERS && !changed; i++)
			if (!strcmp(tok,modifiers[i].name))
			{
				*mod |= modifiers[i].modifier;
				changed = TRUE;
			}
		if (!changed && strncmp(tok, "Button", strlen("Button")) == 0)
		{
			int but;
			if (sscanf(tok, "Button%d", &but) == 1)
			{
				*button = but;
			}
		}
		tok = strtok(NULL,"+");
	}
}


static void
parseActionOption(BSSetting *s, xmlDoc *doc, xmlNode *node)
{
	xmlNode *cur = node;
	while (cur != NULL)
	{
		if (!xmlStrcmp(cur->name, (const xmlChar *) "key")) {
			xmlChar *key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			s->info.forAction.key = TRUE;
			if (key && strlen((char *)key))
			{
				convertKey((char *)key,
							&s->defaultValue.value.asAction.keyModMask,
							&s->defaultValue.value.asAction.keysym);
			}
			xmlFree(key);
		} else if (!xmlStrcmp(cur->name, (const xmlChar *) "mouse")) {
			xmlChar *key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			s->info.forAction.button = TRUE;
			if (key && strlen((char *)key))
			{
				convertButton((char *)key,
							&s->defaultValue.value.asAction.buttonModMask,
							&s->defaultValue.value.asAction.button);
			}
			xmlFree(key);
		} else if (!xmlStrcmp(cur->name, (const xmlChar *) "edge")) {
			xmlChar *key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			s->info.forAction.edge = TRUE;
			char *tok = strtok((char *)key,",");
			while (tok && key)
			{
				if (!strcasecmp(tok,"left"))
					s->defaultValue.value.asAction.edgeMask |= (1 << SCREEN_EDGE_LEFT);
				else if (!strcasecmp(tok,"right"))
					s->defaultValue.value.asAction.edgeMask |= (1 << SCREEN_EDGE_RIGHT);
				else if (!strcasecmp(tok,"top"))
					s->defaultValue.value.asAction.edgeMask |= (1 << SCREEN_EDGE_TOP);
				else if (!strcasecmp(tok,"bottom"))
					s->defaultValue.value.asAction.edgeMask |= (1 << SCREEN_EDGE_BOTTOM);
				else if (!strcasecmp(tok,"topleft"))
					s->defaultValue.value.asAction.edgeMask |= (1 << SCREEN_EDGE_TOPLEFT);
				else if (!strcasecmp(tok,"topright"))
					s->defaultValue.value.asAction.edgeMask |= (1 << SCREEN_EDGE_TOPRIGHT);
				else if (!strcasecmp(tok,"bottomleft"))
					s->defaultValue.value.asAction.edgeMask |= (1 << SCREEN_EDGE_BOTTOMLEFT);
				else if (!strcasecmp(tok,"bottomright"))
					s->defaultValue.value.asAction.edgeMask |= (1 << SCREEN_EDGE_BOTTOMRIGHT);
				tok = strtok(NULL,",");
			}
			xmlFree(key);
		} else if (!xmlStrcmp(cur->name, (const xmlChar *) "bell")) {
			xmlChar *key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			s->info.forAction.bell = TRUE;
			if (key)
				s->defaultValue.value.asAction.onBell = (strcasecmp((char *)key,"true") == 0);
			xmlFree(key);
		}
		cur = cur->next;
	}
}


static void
parseOption(BSPlugin *plugin, xmlDoc *doc, ParserState* pState, xmlNode *node)
{
	xmlNode *cur = node;
	xmlChar *name;
	name = xmlGetProp(cur, (xmlChar *)"name");
	if (!name)
	{
		fprintf(stderr,"[ERROR]: no Option name defined\n");
		xmlFree(name);
		return;
	}
	if (!nameCheck((char *)name))
	{
		fprintf(stderr,"[ERROR]: wrong Option name \"%s\"\n",name);
		xmlFree(name);
		return;
	}
	xmlChar *type;
	type = xmlGetProp(cur, (xmlChar *)"type");
	if (!type)
	{
		fprintf(stderr,"[ERROR]: no Option type defined\n");
		xmlFree(name);
		xmlFree(type);
		return;
	}

	OptionType sType = getOptionType(type);

	if (sType == OptionTypeError)
	{
		fprintf(stderr,"[ERROR]: wrong Option type \"%s\"\n",(char *)type);
		xmlFree(name);
		xmlFree(type);
		return;
	}
	if (sType == OptionTypeAction && pState->screen)
	{
		fprintf(stderr,"[ERROR]: action defination in screen section\n");
		xmlFree(name);
		xmlFree(type);
		return;
	}
	xmlFree(type);

	if (bsFindSetting(plugin, (char *)name, pState->screen, 0))
	{
		fprintf(stderr,"[ERROR]: Option \"%s\" already defined\n",name);
		xmlFree(name);
		return;
	}

	NEW(BSSetting, setting);

	setting->parent     = plugin;
	setting->isScreen   = pState->screen;
	setting->screenNum  = 0;
	setting->isDefault  = TRUE;
	setting->name       = strdup((char *) name);

	if (pState->group)
	{
		setting->group      = strdup(pState->group);
		if (pState->subGroup)
			setting->subGroup   = strdup(pState->subGroup);
		else
			setting->subGroup   = strdup("");
	}
	else
	{
		setting->group      = strdup("");
		setting->subGroup   = strdup("");
	}

	cur = node->xmlChildrenNode;

	while (cur != NULL)
	{
		if (!xmlStrcmp(cur->name, (const xmlChar *) "short") && !setting->shortDesc) {
			xmlChar *key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			setting->shortDesc = strdup((char *)key);
			xmlFree(key);
		} else
		if (!xmlStrcmp(cur->name, (const xmlChar *) "long") && !setting->longDesc) {
			xmlChar *key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			setting->longDesc = strdup((char *)key);
			xmlFree(key);
		}
		if (!xmlStrcmp(cur->name, (const xmlChar *) "hints") && !setting->hints) {
			xmlChar *key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			setting->hints = strdup((char *)key);
			xmlFree(key);
		}
		cur = cur->next;
	}
	xmlFree(name);

	switch (sType)
	{
		case OptionTypeInt:
			setting->type = TypeInt;
			parseIntOption(setting, doc, node->xmlChildrenNode);
			break;
		case OptionTypeBool:
			setting->type = TypeBool;
			parseBoolOption(setting, doc, node->xmlChildrenNode);
			break;
		case OptionTypeFloat:
			setting->type = TypeFloat;
			parseFloatOption(setting, doc, node->xmlChildrenNode);
			break;
		case OptionTypeString:
			setting->type = TypeString;
			parseStringOption(setting, doc, node->xmlChildrenNode);
			break;
		case OptionTypeStringList:
			setting->type = TypeList;
			parseStringListOption(setting, doc, node->xmlChildrenNode);
			break;
		case OptionTypeEnum:
			setting->type = TypeString;
			parseEnumOption(setting, doc, node->xmlChildrenNode);
			break;
		case OptionTypeSelection:
			setting->type = TypeList;
			parseSelectionOption(setting, doc, node->xmlChildrenNode);
			break;
		case OptionTypeColor:
			setting->type = TypeColor;
			parseColorOption(setting, doc, node->xmlChildrenNode);
			break;
		case OptionTypeAction:
			setting->type = TypeAction;
			parseActionOption(setting, doc, node->xmlChildrenNode);
			break;
		case OptionTypeMatch:
			setting->type = TypeMatch;
			parseMatchOption(setting, doc, node->xmlChildrenNode);
			break;
		default:
			break;
	}
	setting->value = &setting->defaultValue;
	setting->defaultValue.parent = setting;

	plugin->settings = bsSettingListAppend(plugin->settings, setting);
}

static void
parseSubGroup(BSPlugin *p, xmlDoc *doc, ParserState* pState, xmlNode *node)
{
	xmlNode *cur = node;
	xmlChar *name;
	name = xmlGetProp(cur, (xmlChar *)"name");
	if (!name)
	{
		fprintf(stderr,"[ERROR]: no subgroup name defined\n");
		xmlFree(name);
		return;
	}
	if (pState->subGroup)
	{
		fprintf(stderr,"[ERROR]: recursive subgroup defined\n");
		xmlFree(name);
		return;
	}


	pState->subGroup = (char *)name;
	parseElements(p, doc, pState, node->xmlChildrenNode);
	pState->subGroup = NULL;
	xmlFree(name);
}

static void
parseGroup(BSPlugin *p, xmlDoc *doc, ParserState* pState, xmlNode *node)
{
	xmlNode *cur = node;
	xmlChar *name;
	name = xmlGetProp(cur, (xmlChar *)"name");
	if (!name)
	{
		fprintf(stderr,"[ERROR]: no group name defined\n");
		xmlFree(name);
		return;
	}
	if (pState->group)
	{
		fprintf(stderr,"[ERROR]: recursive group defined\n");
		xmlFree(name);
		return;
	}
	if (pState->subGroup)
	{
		fprintf(stderr,"[ERROR]: no group defination inside of subgroup definition allowed\n");
		xmlFree(name);
		return;
	}

	pState->group = (char *)name;
	parseElements(p, doc, pState, node->xmlChildrenNode);
	pState->group = NULL;
	xmlFree(name);
}

static void
parseElements(BSPlugin *p, xmlDoc *doc, ParserState* pState, xmlNode *node)
{
	xmlNode *cur = node;
	while (cur != NULL)
	{
		if (!xmlStrcmp(cur->name, (const xmlChar *) "group")) {
			parseGroup(p, doc, pState, cur);
		} else
		if (!xmlStrcmp(cur->name, (const xmlChar *) "subgroup")) {
			parseSubGroup(p, doc, pState, cur);
		} else
		if (!xmlStrcmp(cur->name, (const xmlChar *) "option")) {
			parseOption(p, doc, pState, cur);
		}
		cur = cur->next;
	}

}

static void parseXmlData(BSPlugin *p, xmlDoc *doc)
{
	xmlNode *root_element = NULL;
	ParserState pState;
	pState.screen = FALSE;
	pState.group = NULL;
	pState.subGroup = NULL;

	root_element = xmlDocGetRootElement(doc);

	if (root_element == NULL) {
		fprintf(stderr,"[ERROR]: empty document\n");
		return;
	}
	if (xmlStrcmp(root_element->name, (const xmlChar *) "plugin")) {
		fprintf(stderr,"[ERROR]: document of the wrong type, root node != plugin");
		return;
	}

	xmlNode *pc = root_element->xmlChildrenNode;

	while (pc != NULL)
	{
		if (!xmlStrcmp(pc->name, (const xmlChar *)"screen"))
		{
			pState.screen = TRUE;
			parseElements(p, doc, &pState, pc->xmlChildrenNode);
		} else
		if (!xmlStrcmp(pc->name, (const xmlChar *)"display"))
		{
			pState.screen = FALSE;
			parseElements(p, doc, &pState, pc->xmlChildrenNode);
		} else
		if (!xmlStrcmp(pc->name, (const xmlChar *) "short") && !p->shortDesc) {
			xmlChar *key = xmlNodeListGetString(doc, pc->xmlChildrenNode, 1);
			p->shortDesc = strdup((char *)key);
			xmlFree(key);
		} else
		if (!xmlStrcmp(pc->name, (const xmlChar *) "long") && !p->longDesc) {
			xmlChar *key = xmlNodeListGetString(doc, pc->xmlChildrenNode, 1);
			p->longDesc = strdup((char *)key);
			xmlFree(key);
		} else
		if (!xmlStrcmp(pc->name, (const xmlChar *) "category") && !p->category) {
			xmlChar *key = xmlNodeListGetString(doc, pc->xmlChildrenNode, 1);
			p->category = strdup((char *)key);
			xmlFree(key);
		}
		pc = pc->next;
	}
}

void bsLoadPlugin(BSContext * context, char * filename)
{
	xmlDoc *doc = NULL;

	char *dir = dirname(strdup(filename));
	char *base = basename(strdup(filename));
	char *xmlname;
	char name[1024];

	sscanf(base, "lib%s", name);
        if (strlen(name) > 3)
	   name[strlen(name) - 3] = 0;

	asprintf(&xmlname,"%s/%s.options",dir,name);

	NEW(BSPlugin,plugin);

	plugin->context = context;
	plugin->name    = strdup(name);

	if (!access(xmlname,R_OK))
	{
		 doc = xmlReadFile(xmlname, NULL, 0);
		 if (doc)
		 {
			 printf("Parsing plugin metadata \"%s\"\n",xmlname);
			 parseXmlData(plugin, doc);
		 }
	}

	if (!doc)
		printf("No metadata found for plugin \"%s\".\n",name);

	if (!plugin->shortDesc)
		plugin->shortDesc = strdup(name);
	if (!plugin->longDesc)
		plugin->longDesc  = strdup(filename);
	plugin->filename  = strdup(filename);

	if (plugin->category)
		plugin->category  = strdup("");

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

	collateGroups(plugin);
	context->plugins = bsPluginListAppend(context->plugins, plugin);
}

