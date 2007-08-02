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

#define _GNU_SOURCE
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include <ccs.h>
#include "iniparser.h"

/** 
 * Creates the parent directory for @fileName, recursively creating a directory
 * tree if necessary.
 *
 * @param fileName: The absolute path to the desired file
 * @return: True if the parent directory of the file now exists
**/

static Bool ccsCreateDirFor(const char *fileName)
{
    char *path, *delim;
    Bool success;

    delim = strrchr (fileName, '/');
    if (!delim)
    	return FALSE;	/* Input string is not a valid absolue path! */

    path = malloc (delim - fileName + 1);
    if (!path)
    	return FALSE;

    strncpy (path, fileName, delim - fileName);
    path[delim - fileName] = '\0';

    success = !mkdir (path, 0700);	/* Mkdir returns 0 on success */
    success |= (errno == EEXIST);
    if (!success && (errno == ENOENT))	/* ENOENT means we must recursively */
    {					/* create the parent's parent */
	if (ccsCreateDirFor (path));
	    success = !mkdir (path, 0700);
    }
    free (path);
    return success;
}    

IniDictionary * ccsIniOpen (const char * fileName)
{
    FILE *file;

    if (!ccsCreateDirFor(fileName))
	return NULL;

    /* create file if it doesn't exist or is desired */
    file = fopen (fileName, "a+");
    if (file)
	fclose (file);

    return iniparser_new ((char*) fileName);
}

IniDictionary*
ccsIniNew (void)
{
    return dictionary_new (0);
}

void
ccsIniClose (IniDictionary *dictionary)
{
    iniparser_free (dictionary);
}

void
ccsIniSave (IniDictionary *dictionary,
	    const char    *fileName)
{
    if (!ccsCreateDirFor (fileName))
	return;

    iniparser_dump_ini (dictionary, fileName);
}

static char*
getIniString (IniDictionary *dictionary,
	      const char    *section,
	      const char    *entry)
{
    char *sectionName;
    char *retValue;

    asprintf (&sectionName, "%s:%s", section, entry);

    retValue = iniparser_getstring (dictionary, sectionName, NULL);
    free (sectionName);

    return retValue;
}

static void
setIniString (IniDictionary *dictionary,
	      const char    *section,
	      const char    *entry,
	      const char    *value)
{
    char *sectionName;

    asprintf (&sectionName, "%s:%s", section, entry);

    if (!iniparser_find_entry (dictionary, (char*) section))
	iniparser_add_entry (dictionary, (char*) section, NULL, NULL);

    iniparser_setstr (dictionary, sectionName, (char*) value);

    free (sectionName);
}

static char*
writeActionString (CCSSettingActionValue * action)
{
    char          *keyBinding;
    char          *buttonBinding;
    char          *actionString = NULL;
    char          edgeString[500];
    CCSStringList edgeList, l;

    keyBinding = ccsKeyBindingToString (action);
    if (!keyBinding)
	keyBinding = strdup ("");

    buttonBinding = ccsButtonBindingToString (action);
    if (!buttonBinding)
	buttonBinding = strdup ("");

    edgeList = ccsEdgesToStringList (action);
    memset (edgeString, 0, sizeof (edgeString));

    for (l = edgeList; l; l = l->next)
    {
	strncat (edgeString, l->data, 500);
	if (l->next)
	    strncat (edgeString, "|", 500);
    }

    if (edgeList)
	ccsStringListFree (edgeList, TRUE);

    asprintf (&actionString, "%s,%s,%s,%d,%s", keyBinding,
	      buttonBinding, edgeString, action->edgeButton,
	      action->onBell ? "true" : "false");

    free (keyBinding);
    free (buttonBinding);

    return actionString;
}

static Bool
parseActionString (const char            *string,
    		   CCSSettingActionValue *value)
{
    char          *valueString, *valueStart;
    char          *token, *edgeToken;
    CCSStringList edgeList = NULL;

    memset (value, 0, sizeof (CCSSettingActionValue));
    valueString = strdup (string);
    valueStart = valueString;

    token = strsep (&valueString, ",");
    if (!token)
    {
	free (valueStart);
	return FALSE;
    }

    /* key binding */
    ccsStringToKeyBinding (token, value);
    token = strsep (&valueString, ",");
    if (!token)
    {
	free (valueStart);
	return FALSE;
    }

    /* button binding */
    ccsStringToButtonBinding (token, value);
    token = strsep (&valueString, ",");
    if (!token)
    {
	free (valueStart);
	return FALSE;
    }

    /* edge binding */
    edgeToken = strsep (&token, "|");
    while (edgeToken)
    {
	if (strlen (edgeToken))
	    edgeList = ccsStringListAppend (edgeList, strdup (edgeToken));
	edgeToken = strsep (&token, "|");
    }

    ccsStringListToEdges (edgeList, value);
    if (edgeList)
	ccsStringListFree (edgeList, TRUE);

    token = strsep (&valueString, ",");
    if (!token)
    {
	free (valueStart);
	return FALSE;
    }

    /* edge button */
    value->edgeButton = strtoul (token, NULL, 10);

    /* bell */
    value->onBell = (strcmp (valueString, "true") == 0);

    return TRUE;
}

Bool
ccsIniGetString (IniDictionary *dictionary,
	    	 const char    *section,
		 const char    *entry,
		 char          **value)
{
    char *retValue;

    retValue = getIniString (dictionary, section, entry);
    if (retValue)
    {
	*value = strdup (retValue);
	return TRUE;
    }
    else
	return FALSE;
}

Bool
ccsIniGetInt (IniDictionary *dictionary,
	      const char    *section,
	      const char    *entry,
	      int           *value)
{
    char *retValue;

    retValue = getIniString (dictionary, section, entry);
    if (retValue)
    {
	*value = strtoul (retValue, NULL, 10);
	return TRUE;
    }
    else
	return FALSE;
}

Bool
ccsIniGetFloat (IniDictionary *dictionary,
		const char    *section,
		const char    *entry,
		float         *value)
{
    char *retValue;

    retValue = getIniString (dictionary, section, entry);
    if (retValue)
    {
	*value = (float) strtod (retValue, NULL);
	return TRUE;
    }
    else
	return FALSE;
}

Bool
ccsIniGetBool (IniDictionary *dictionary,
	       const char    *section,
   	       const char    *entry,
	       Bool          *value)
{
    char *retValue;

    retValue = getIniString (dictionary, section, entry);
    if (retValue)
    {
	if ((retValue[0] == 't') || (retValue[0] == 'T') ||
	    (retValue[0] == 'y') || (retValue[0] == 'Y') ||
	    (retValue[0] == '1'))
	{
	    *value = TRUE;
	}
	else
	    *value = FALSE;

	return TRUE;
    }
    else
	return FALSE;
}

Bool
ccsIniGetColor (IniDictionary        *dictionary,
		const char           *section,
		const char           *entry,
		CCSSettingColorValue *value)
{
    char *retValue;

    retValue = getIniString (dictionary, section, entry);
    if (retValue && ccsStringToColor (retValue, value))
	return TRUE;
    else
	return FALSE;
}

Bool
ccsIniGetAction (IniDictionary         *dictionary,
	   	 const char            *section,
   		 const char            *entry,
		 CCSSettingActionValue *value)
{
    char *retValue;

    retValue = getIniString (dictionary, section, entry);
    if (retValue)
	return parseActionString (retValue, value);
    else
	return FALSE;
}

Bool
ccsIniGetList (IniDictionary       *dictionary,
   	       const char          *section,
	       const char          *entry,
	       CCSSettingValueList *value,
	       CCSSetting          *parent)
{
    CCSSettingValueList list = NULL;
    char                *valueString, *valueStart, *valString;
    char                *token;
    int                 nItems = 1, i = 0;

    valString = getIniString (dictionary, section, entry);
    if (!valString)
	return FALSE;

    valueString = strdup (valString);
    valueStart = valueString;

    token = strchr (valueString, ';');
    while (token)
    {
	token = strchr (token + 1, ';');
	nItems++;
    }

    token = strsep (&valueString, ";");
    switch (parent->info.forList.listType)
    {
    case TypeString:
    case TypeMatch:
	{
	    char **array = malloc (nItems * sizeof (char*));
	    while (token)
	    {
		array[i++] = strdup (token);
		token = strsep (&valueString, ";");
	    }

	    list = ccsGetValueListFromStringArray (array, nItems, parent);

	    for (i = 0; i < nItems; i++)
		free (array[i]);

	    free (array);
	}
	break;
    case TypeColor:
	{
	    CCSSettingColorValue *array;
	    array = malloc (nItems * sizeof (CCSSettingColorValue));
	    while (token)
	    {
		memset (&array[i], 0, sizeof (CCSSettingColorValue));
		ccsStringToColor (token, &array[i]);
		token = strsep (&valueString, ";");
		i++;
	    }

	    list = ccsGetValueListFromColorArray (array, nItems, parent);
	    free (array);
	}
	break;
    case TypeBool:
	{
	    Bool *array = malloc (nItems * sizeof (Bool));
	    Bool isTrue;
	    while (token)
	    {
		isTrue = (token[0] == 'y' || token[0] == 'Y' || 
			  token[0] == '1' ||
			  token[0] == 't' || token[0] == 'T');
		array[i++] = isTrue;
		token = strsep (&valueString, ";");
	    }

	    list = ccsGetValueListFromBoolArray (array, nItems, parent);
	    free (array);
	}
	break;
    case TypeInt:
	{
	    int *array = malloc (nItems * sizeof (int));
	    while (token)
	    {
		array[i++] = strtoul (token, NULL, 10);
		token = strsep (&valueString, ";");
	    }

	    list = ccsGetValueListFromIntArray (array, nItems, parent);
	    free (array);
	}
	break;
    case TypeFloat:
	{
	    float *array = malloc (nItems * sizeof (float));
	    while (token)
	    {
		array[i++] = strtod (token, NULL);
		token = strsep (&valueString, ";");
	    }

	    list = ccsGetValueListFromFloatArray (array, nItems, parent);
	    free (array);
	}
	break;
    case TypeAction:
	{
	    CCSSettingActionValue *array;
	    array = malloc (nItems * sizeof (CCSSettingActionValue));
	    while (token)
	    {
		parseActionString (token, &array[i++]);
		token = strsep (&valueString, ";");
	    }

	    list = ccsGetValueListFromActionArray (array, nItems, parent);
	    free (array);
	}
	break;
    default:
	break;
    }

    *value = list;
    free (valueStart);

    return TRUE;
}

void
ccsIniSetString (IniDictionary * dictionary,
		 const char    * section,
		 const char    * entry,
		 char          * value)
{
    setIniString (dictionary, section, entry, value);
}

void
ccsIniSetInt (IniDictionary *dictionary,
	      const char    *section,
	      const char    *entry,
	      int           value)
{
    char *string = NULL;

    asprintf (&string, "%i", value);
    if (string)
    {
	setIniString (dictionary, section, entry, string);
	free (string);
    }
}

void
ccsIniSetFloat (IniDictionary *dictionary,
		const char    *section,
		const char    *entry,
		float         value)
{
    char *string = NULL;

    asprintf (&string, "%f", value);
    if (string)
    {
	setIniString (dictionary, section, entry, string);
	free (string);
    }
}

void
ccsIniSetBool (IniDictionary *dictionary,
	       const char    *section,
	       const char    *entry,
	       Bool          value)
{
    setIniString (dictionary, section, entry,
		  value ? "true" : "false");
}

void
ccsIniSetColor (IniDictionary        *dictionary,
		const char           *section,
	   	const char           *entry,
   		CCSSettingColorValue value)
{
    char *string;

    string = ccsColorToString (&value);
    if (string)
    {
	setIniString (dictionary, section, entry, string);
	free (string);
    }
}

void
ccsIniSetAction (IniDictionary         *dictionary,
		 const char            *section,
		 const char            *entry,
		 CCSSettingActionValue value)
{
    char *actionString;

    actionString = writeActionString (&value);
    if (actionString)
    {
	setIniString (dictionary, section, entry, actionString);
	free (actionString);
    }
}

void
ccsIniSetList (IniDictionary       *dictionary,
	       const char          *section,
	       const char          *entry,
	       CCSSettingValueList value,
	       CCSSettingType      listType)
{
#define STRINGBUFSIZE 2048
    /* FIXME: We should allocate that dynamically */
    char stringBuffer[STRINGBUFSIZE];

    memset (stringBuffer, 0, sizeof (stringBuffer));

    while (value)
    {
	switch (listType)
	{
	case TypeString:
	    strncat (stringBuffer, value->data->value.asString, STRINGBUFSIZE);
	    break;
	case TypeMatch:
	    strncat (stringBuffer, value->data->value.asMatch, STRINGBUFSIZE);
	    break;
	case TypeInt:
	    {
		char *valueString = NULL;
		asprintf (&valueString, "%d", value->data->value.asInt);
		if (!valueString)
		    break;

		strncat (stringBuffer, valueString, STRINGBUFSIZE);
		free (valueString);
	    }
	    break;
	case TypeBool:
	    strncat (stringBuffer,
		     (value->data->value.asBool) ? "true" : "false",
		     STRINGBUFSIZE);
	    break;
	case TypeFloat:
	    {
		char *valueString = NULL;
		asprintf (&valueString, "%f", value->data->value.asFloat);
		if (!valueString)
		    break;

		strncat (stringBuffer, valueString, STRINGBUFSIZE);
		free (valueString);
	    }
	    break;
	case TypeColor:
	    {
		char *color = NULL;
		color = ccsColorToString (&value->data->value.asColor);
		if (!color)
		    break;

		strncat (stringBuffer, color, STRINGBUFSIZE);
		free (color);
	    }
	    break;
	case TypeAction:
	    {
		char *action;
		action = writeActionString (&value->data->value.asAction);
		if (!action)
		    break;

		strncat (stringBuffer, action, STRINGBUFSIZE);
		free (action);
	    }
	default:
	    break;
	}

	if (value->next)
	    strncat (stringBuffer, ";", STRINGBUFSIZE);

	value = value->next;
    }

    setIniString (dictionary, section, entry, stringBuffer);
}

void ccsIniRemoveEntry (IniDictionary * dictionary,

			const char * section,
			const char * entry)
{
    char *sectionName;

    asprintf (&sectionName, "%s:%s", section, entry);
    iniparser_unset (dictionary, sectionName);
    free (sectionName);
}
