#define _GNU_SOURCE
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include <bsettings.h>
#include "iniparser.h"

IniDictionary * bsIniOpen (const char * fileName)
{
	char *path, *delim;
	FILE *file;

    path = strdup (fileName);
    delim = strrchr (path, '/');
    if (delim)
	*delim = 0;

    if (!mkdir (path, 0777) && (errno != EEXIST))
    {
		free (path);
		return NULL;
    }
    free (path);

	/* create file if it doesn't exist */
	file = fopen (fileName, "a+");
	fclose (file);

	return iniparser_new ((char*) fileName);
}

void bsIniClose (IniDictionary * dictionary)
{
	iniparser_free (dictionary);
}

void bsIniSave (IniDictionary * dictionary, const char * fileName)
{
	FILE *file;
	char *path, *delim;

	path = strdup (fileName);
	delim = strrchr (path, '/');
	if (delim)
		*delim = 0;

	if (!mkdir (path, 0777) && (errno != EEXIST))
	{
		free (path);
		return;
	}
	free (path);

	file = fopen (fileName, "w");
	iniparser_dump_ini (dictionary, file);
	fclose (file);
}

static char *getIniString (IniDictionary * dictionary,
						   const char * section,
						   const char * entry)
{
	char *sectionName;
	char *retValue;

	asprintf (&sectionName, "%s:%s", section, entry);

	retValue = iniparser_getstring (dictionary, sectionName, NULL);
	free (sectionName);

	return retValue;
}

static void setIniString (IniDictionary * dictionary,
	   					  const char * section,
   						  const char * entry,
						  const char * value)
{
	char *sectionName;

	asprintf (&sectionName, "%s:%s", section, entry);

	if (!iniparser_find_entry (dictionary, (char*) section))
	    iniparser_add_entry (dictionary, (char*) section, NULL, NULL);

	iniparser_setstr (dictionary, sectionName, (char*) value);
	free (sectionName);
}

static char *writeActionString (BSSettingActionValue * action)
{
	char *keyBinding;
	char *buttonBinding;
	char *edge;
	char *actionString = NULL;

	keyBinding = bsKeyBindingToString (action);
	if (!keyBinding)
		keyBinding = strdup("");

	buttonBinding = bsButtonBindingToString (action);
	if (!buttonBinding)
		buttonBinding = strdup("");

	edge = bsEdgeToString (action);
	if (!edge)
		edge = strdup("");

	asprintf (&actionString, "%s,%s,%s,%d,%s\n", keyBinding,
			  buttonBinding, edge, action->edgeButton,
			  action->onBell ? "true" : "false");

	free (keyBinding);
	free (buttonBinding);
	free (edge);

	return actionString;
}

static Bool parseActionString (const char* string, 
							   BSSettingActionValue *value)
{
	char *valueString, *valueStart;
	char *token;

	memset (value, 0, sizeof(BSSettingActionValue));
	valueString = strdup (string);
	valueStart = valueString;

	token = strsep (&valueString, ",");
	if (!token)
	{
		free (valueStart);
		return FALSE;
	}

	/* key binding */
	bsStringToKeyBinding (token, value);

	token = strsep (&valueString, ",");
	if (!token)
	{
		free (valueStart);
		return FALSE;
	}

	/* button binding */
	bsStringToButtonBinding (token, value);

	token = strsep (&valueString, ",");
	if (!token)
	{
		free (valueStart);
		return FALSE;
	}

	/* edge binding */
	bsStringToEdge (token, value);

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

Bool bsIniGetString (IniDictionary * dictionary, 
					 const char * section,
					 const char * entry, 
					 char ** value)
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

Bool bsIniGetInt (IniDictionary * dictionary, 
				  const char * section,
				  const char * entry, 
				  int * value)
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

Bool bsIniGetFloat (IniDictionary * dictionary, 
					const char * section,
					const char * entry, 
					float * value)
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

Bool bsIniGetBool (IniDictionary * dictionary, 
				   const char * section,
				   const char * entry, 
				   Bool * value)
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

Bool bsIniGetColor (IniDictionary * dictionary, 
					const char * section,
					const char * entry, 
					BSSettingColorValue * value)
{
	char *retValue;

	retValue = getIniString (dictionary, section, entry);

	if (retValue && bsStringToColor (retValue, value))
		return TRUE;
	else
		return FALSE;
}

Bool bsIniGetAction (IniDictionary * dictionary, 
					 const char * section,
					 const char * entry, 
					 BSSettingActionValue * value)
{
	char *retValue;

	retValue = getIniString (dictionary, section, entry);

	if (retValue)
		return parseActionString (retValue, value);
	else
		return FALSE;
}

Bool bsIniGetList (IniDictionary * dictionary, 
				   const char * section,
				   const char * entry, 
				   BSSettingValueList * value,
				   BSSetting *parent)
{
	BSSettingValueList list = NULL;
	char *valueString, *valueStart, *valString;
	char *token;
	int nItems = 0, i = 0;

	valString = getIniString (dictionary, section, entry);
	if (!valString)
		return FALSE;

	valueString = strdup (valString);
	valueStart = valueString;

	token = strsep (&valueString, ";");
	while (token)
	{
		token = strsep (&valueString, ";");
		nItems++;
	}
	valueString = valueStart;

	token = strsep (&valueString, ";");

	switch (parent->info.forList.listType)
	{
		case TypeString:
		case TypeMatch:
			{
				char **array = malloc (nItems * sizeof(char*));
				while (token)
				{
					array[i++] = strdup (token);
					token = strsep (&valueString, ";");
				}
				list = bsGetValueListFromStringArray (array, nItems, parent);
				for (i = 0; i < nItems; i++)
					free (array[i]);
				free (array);
			}
			break;
		case TypeColor:
			{
				BSSettingColorValue *array = malloc (nItems * sizeof(BSSettingColorValue));
				while (token)
				{
                    memset(&array[i], 0, sizeof(BSSettingColorValue));
                   	bsStringToColor(token, &array[i]);
					token = strsep (&valueString, ";");
					i++;
				}
				list = bsGetValueListFromColorArray (array, nItems, parent);
				free (array);
			}
			break;
		case TypeBool:
			{
				Bool *array = malloc (nItems * sizeof(Bool));
				Bool isTrue;
				while (token)
				{
					isTrue = (token[0] == 'y' || token[0] == 'Y' || token[0] == '1' ||
							  token[0] == 't' || token[0] == 'T');
					array[i++] = isTrue;
					token = strsep (&valueString, ";");
				}
				list = bsGetValueListFromBoolArray (array, nItems, parent);
				free (array);
			}
			break;
		case TypeInt:
			{
				int *array = malloc (nItems * sizeof(int));
				while (token)
				{
					array[i++] = strtoul (token, NULL, 10);
					token = strsep (&valueString, ";");
				}
				list = bsGetValueListFromIntArray (array, nItems, parent);
				free (array);
			}
			break;
		case TypeFloat:
			{
				float *array = malloc (nItems * sizeof(float));
				while (token)
				{
					array[i++] = strtod (token, NULL);
					token = strsep (&valueString, ";");
				}
				list = bsGetValueListFromFloatArray (array, nItems, parent);
				free (array);
			}
			break;
		case TypeAction:
			{
				BSSettingActionValue *array = malloc (nItems * sizeof(BSSettingActionValue));
				while (token)
				{
					parseActionString (token, &array[i++]);
					token = strsep (&valueString, ";");
				}
				list = bsGetValueListFromActionArray (array, nItems, parent);
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

void bsIniSetString (IniDictionary * dictionary, 
					 const char * section,
					 const char * entry, 
					 char * value)
{
	setIniString (dictionary, section, entry, value);
}

void bsIniSetInt (IniDictionary * dictionary, 
				  const char * section,
				  const char * entry, 
				  int value)
{
	char *string = NULL;
	asprintf (&string, "%i", value);

	if (string)
	{
		setIniString (dictionary, section, entry, string);
		free (string);
	}
}

void bsIniSetFloat (IniDictionary * dictionary, 
					const char * section,
					const char * entry, 
					float value)
{
	char *string = NULL;
	asprintf (&string, "%f", value);

	if (string)
	{
		setIniString (dictionary, section, entry, string);
		free (string);
	}
}

void bsIniSetBool (IniDictionary * dictionary, 
				   const char * section,
				   const char * entry, 
				   Bool value)
{
	setIniString (dictionary, section, entry,
				  value ? "true" : "false");
}

void bsIniSetColor (IniDictionary * dictionary, 
					const char * section,
					const char * entry, 
					BSSettingColorValue value)
{
	char *string;
	string = bsColorToString (&value);

	if (string)
	{
		setIniString (dictionary, section, entry, string);
		free (string);
	}
}

void bsIniSetAction (IniDictionary * dictionary, 
					 const char * section,
					 const char * entry, 
					 BSSettingActionValue value)
{
	char *actionString;

	actionString = writeActionString (&value);

	if (actionString)
	{
		setIniString (dictionary, section, entry, actionString);
		free (actionString);
	}
}

void bsIniSetList (IniDictionary * dictionary, 
				   const char * section,
				   const char * entry, 
				   BSSettingValueList value,
				   BSSettingType listType)
{
#define STRINGBUFSIZE 2048
	char stringBuffer[STRINGBUFSIZE]; //FIXME: we should allocate that dynamically

	memset (stringBuffer, 0, sizeof(stringBuffer));

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
				strncat (stringBuffer, (value->data->value.asBool) ? "true" : "false", 
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
					color = bsColorToString(&value->data->value.asColor);
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

		strncat (stringBuffer, ";", STRINGBUFSIZE);;
		value = value->next;
	}

	setIniString (dictionary, section, entry, stringBuffer);
}

void bsIniRemoveEntry (IniDictionary * dictionary, 
					   const char * section,
					   const char * entry)
{
	char *sectionName;

	asprintf (&sectionName, "%s:%s", section, entry);
	iniparser_unset (dictionary, sectionName);
	free (sectionName);
}
