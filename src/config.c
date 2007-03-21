#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include "bsettings-private.h"

static BSStringList readFile(void)
{
	char * home = getenv("HOME");

	if (!home && strlen(home))
		return NULL;

	char *filename = NULL;
	asprintf(&filename, "%s/.bsettings/config",home);
	
	FILE *f = fopen(filename,"r");

	free(filename);
	
	if (!f)
		return NULL;

	BSStringList rv = NULL;
	char line[1024];
	while (fscanf(f, "%s\n", line) != EOF)
	{
		if (line && strlen(line))
		{
			rv = bsStringListAppend(rv, strdup(line));
		}
	}
	
	fclose(f);
	return rv;
}

static Bool writeFile(BSStringList list)
{
	char * home = getenv("HOME");

	if (!home && strlen(home))
		return FALSE;

	char *filename = NULL;
	asprintf(&filename, "%s/.bsettings/config",home);

	char *dir = NULL;
	asprintf(&dir, "%s/.bsettings",home);

	
	if (!mkdir(dir,0777) && errno != EEXIST)
	{
		free(filename);
		free(dir);
		return FALSE;
	}
	free (dir);
	
	FILE *f = fopen(filename,"w");

	free(filename);
	
	if (!f)
		return FALSE;

	while (list)
	{
		fprintf(f, "%s\n",list->data);
		list = list->next;
	}
	fclose(f);
	return TRUE;
}

Bool bsReadConfig(ConfigOption option, char** value)
{
	BSStringList list = readFile();
	BSStringList l = list;

	char *search = "no_key_defined";
	switch (option)
	{
		case OptionProfile:
			search = "profile";
			break;
		case OptionBackend:
			search = "backend";
			break;
		case OptionIntegration:
			search = "integration";
			break;
		default:
			break;
	}

	*value = NULL;
	
	if (!list)
		return FALSE;
	while (l)
	{
		char *line = strdup(l->data);
		char *key = strtok(line,"=");
		char *svalue = strtok(NULL,"=");
		if (key && !strcmp(key,search))
		{
			*value = (svalue)? strdup(svalue) : NULL;
			l = NULL;
			continue;
		}
		free(line);
		l = l->next;
	}
	bsStringListFree(list, TRUE);
	if (*value)
		return TRUE;
	return FALSE;
}

Bool bsWriteConfig(ConfigOption option, char* value)
{
	BSStringList list = readFile();
	BSStringList l = list;

	

	char *search = "no_key_defined";
	switch (option)
	{
		case OptionProfile:
			search = "profile";
			break;
		case OptionBackend:
			search = "backend";
			break;
		case OptionIntegration:
			search = "integration";
			break;
		default:
			break;
	}
	
	while (l)
	{
		char *line = strdup(l->data);
		char *key = strtok(line,"=");
		if (key && !strcmp(key,search))
		{
			list = bsStringListRemove(list, l->data, TRUE);
			l = list;
		}
		free(line);
		l = l->next;
	}

	if (value && strlen(value))
	{
		char *newKey = NULL;
		asprintf(&newKey, "%s=%s", search, value);
		list = bsStringListAppend(list, newKey);
	}

	Bool rv = writeFile(list);

	bsStringListFree(list, TRUE);
	return rv;
}

