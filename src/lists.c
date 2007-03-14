#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include <bsettings.h>

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

