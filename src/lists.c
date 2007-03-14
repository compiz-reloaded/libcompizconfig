#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include <bsettings.h>

#include "bsettings-private.h"

typedef void (*freeFunc) (void *ptr);

#define BSLIST(type,dtype) \
\
BS##type##List * bs##type##ListAppend (BS##type##List * list, dtype *data) \
{ \
	BS##type##List * l = list; \
	BS##type##List * ne = malloc(sizeof(BS##type##List)); \
	ne->data = data; \
	ne->next = NULL; \
	if (!list) \
		return ne; \
	while (l->next) l = l->next; \
	l->next = ne; \
	return list; \
} \
\
BS##type##List * bs##type##ListPrepend (BS##type##List * list, dtype *data) \
{ \
	BS##type##List * ne = malloc(sizeof(BS##type##List)); \
	ne->data = data; \
	ne->next = list; \
	return ne; \
} \
\
BS##type##List * bs##type##ListInsert (BS##type##List * list, dtype *data, int position) \
{ \
	BS##type##List * l = list; \
	BS##type##List * ne = malloc(sizeof(BS##type##List)); \
	ne->data = data; \
	ne->next = list; \
	if (!list || !position) \
	    return ne; \
	position--; \
	while (l->next && position) \
	{ \
		l = l->next; \
		position--; \
	} \
	ne->next = l->next; \
	l->next = ne; \
	return list; \
} \
\
BS##type##List * bs##type##ListInsertBefore (BS##type##List * list, BS##type##List * sibling, dtype *data) \
{ \
	BS##type##List * l = list; \
	BS##type##List * ne = malloc(sizeof(BS##type##List)); \
	while (l && (l != sibling)) l = l->next; \
	ne->data = data; \
	ne->next = l; \
	return ne; \
} \
\
unsigned int bs##type##ListLength (BS##type##List * list) \
{ \
	unsigned int count = 0; \
	BS##type##List * l = list; \
	while (l) \
	{ \
		l = l->next; \
		count++; \
	} \
	return count; \
} \
\
BS##type##List * bs##type##ListFind (BS##type##List * list, dtype *data) \
{ \
	BS##type##List * l = list; \
	while (l) \
	{ \
		if (!data && !l->data) break; \
		if (memcmp(l->data, data, sizeof(dtype)) == 0) break; \
		l = l->next; \
	} \
	return l; \
} \
\
BS##type##List * bs##type##ListGetItem (BS##type##List * list, unsigned int index) \
{ \
	BS##type##List * l = list; \
	while (l && index) \
	{ \
		l = l->next; \
		index--; \
	} \
	return l; \
} \
\
BS##type##List * bs##type##ListFree (BS##type##List * list, Bool freeObj) \
{ \
	BS##type##List *l = list; \
	BS##type##List *le = NULL; \
	while (l) \
	{ \
		le = l; \
		l = l->next; \
		if (freeObj) \
			bsFree##type (le->data); \
		free(le); \
	} \
	return NULL; \
}

BSLIST(Plugin,BSPlugin)
BSLIST(Setting,BSSetting)
BSLIST(String,char)
BSLIST(Group,BSGroup)
BSLIST(SubGroup,BSSubGroup)
BSLIST(SettingValue,BSSettingValue)

BSSettingValueList * bsGetValueListFromStringList(BSStringList *list)
{
	BSSettingValueList *rv = NULL;
	while (list)
	{
		NEW(BSSettingValue, value);
		value->isListChild = TRUE;
		value->value.asString = strdup(list->data);
		rv = bsSettingValueListAppend(rv, value);
		list = list->next;
	}
	return rv;
}

BSStringList * bsGetStringListFromValueList(BSSettingValueList *list)
{
	BSStringList *rv = NULL;
	while (list)
	{
		rv = bsStringListAppend(rv, strdup(list->data->value.asString));
		list = list->next;
	}
	return rv;
}

char ** bsGetStringArrayFromList(BSStringList *list, int *num)
{
	char ** rv = NULL;
	int length = bsStringListLength(list);
	int i;
	
	if (length)
		rv = malloc(length * sizeof(char *));

	for (i = 0; i < length; i++, list = list->next)
		rv[i] = strdup(list->data);
	*num = length;
	return rv;
}

BSStringList * bsGetListFromStringArray(char ** array, int num)
{
	BSStringList *rv = NULL;
	int i;
	for (i = 0; i < num; i++)
		rv = bsStringListAppend(rv, strdup(array[i]));
	return rv;
}
