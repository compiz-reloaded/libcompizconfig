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
