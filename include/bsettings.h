#ifndef _BSETTINGS_H
#define _BSETTINGS_H

#ifndef Bool
#define Bool unsigned int
#endif

#ifndef TRUE
#define TRUE ~0
#endif

#ifndef FALSE
#define FALSE 0
#endif

#define BSLIST_HDR(type,dtype)		\
typedef struct _BS##type##List *	BS##type##List;\
struct _BS##type##List	\
{								\
	dtype   * data;			\
	BS##type##List next;		\
}; \
BS##type##List bs##type##ListAppend (BS##type##List list, dtype *data); \
BS##type##List bs##type##ListPrepend (BS##type##List list, dtype *data); \
BS##type##List bs##type##ListInsert (BS##type##List list, dtype *data, int position); \
BS##type##List bs##type##ListInsertBefore (BS##type##List list, BS##type##List sibling, dtype *data); \
unsigned int bs##type##ListLength (BS##type##List list); \
BS##type##List bs##type##ListFind (BS##type##List list, dtype *data); \
BS##type##List bs##type##ListGetItem (BS##type##List list, unsigned int index); \
BS##type##List bs##type##ListRemove (BS##type##List list, dtype *data, Bool freeObj); \
BS##type##List bs##type##ListFree (BS##type##List list, Bool freeObj);

typedef struct _BSContext			BSContext;
typedef struct _BSPlugin			BSPlugin;
typedef struct _BSSetting			BSSetting;
typedef struct _BSGroup				BSGroup;
typedef struct _BSSubGroup			BSSubGroup;
typedef struct _BSBackend			BSBackend;
typedef struct _BSBackendVTable 	BSBackendVTable;
typedef struct _BSPluginCategory	BSPluginCategory;
typedef struct _BSSettingValue		BSSettingValue;

BSLIST_HDR(Plugin,BSPlugin)
BSLIST_HDR(Setting,BSSetting)
BSLIST_HDR(String,char)
BSLIST_HDR(Group,BSGroup)
BSLIST_HDR(SubGroup,BSSubGroup)
BSLIST_HDR(SettingValue,BSSettingValue)


struct _BSContext
{
	BSPluginList		plugins;
	BSPluginCategory *	categories;
	void *				privatePtr;

	BSBackend *			backend;

	char *				profile;
	Bool				deIntegration;

	BSSettingList		changedSettings;
	Bool 				pluginsChanged;
};

struct _BSBackend
{
	void *				dlhand;
	BSBackendVTable *	vTable;
};

typedef BSBackendVTable *(*BackendGetInfoProc) (void);

typedef void (*BSExecuteEventsFunc) (void);

typedef Bool (*BSInitBackendFunc) (BSContext * context);
typedef Bool (*BSFiniBackendFunc) (BSContext * context);

typedef Bool (*BSContextReadInitFunc) (BSContext * context);
typedef void (*BSContextReadSettingFunc)
					(BSContext * context, BSSetting * setting);
typedef void (*BSContextReadDoneFunc) (BSContext * context);

typedef Bool (*BSContextWriteInitFunc) (BSContext * context);
typedef void (*BSContextWriteSettingFunc)
					(BSContext * context, BSSetting * setting);
typedef void (*BSContextWriteDoneFunc) (BSContext * context);

typedef Bool (*BSGetIsIntegratedFunc) (BSSetting * setting);
typedef Bool (*BSGetIsReadOnlyFunc) (BSSetting * setting);

typedef BSStringList (*BSGetExistingProfilesFunc) (void);
typedef Bool (*BSDeleteProfileFunc) (char * name);

struct _BSBackendVTable
{
	char *				name;
	char *				shortDesc;
	char *				longDesc;
	Bool				integrationSupport;
	Bool				profileSupport;

	BSExecuteEventsFunc			executeEvents; // something like a event loop call for the backend
	// so it can check for file changes (gconf changes in the gconf backend)
	// no need for reload settings signals anymore

	BSInitBackendFunc			backendInit;
	BSFiniBackendFunc			backendFini;

   	BSContextReadInitFunc		readInit;
   	BSContextReadSettingFunc	readSetting;
   	BSContextReadDoneFunc		readDone;

   	BSContextWriteInitFunc		writeInit;
   	BSContextWriteSettingFunc	writeSetting;
   	BSContextWriteDoneFunc		writeDone;


	BSGetIsIntegratedFunc 		getSettingIsIntegrated;
	BSGetIsReadOnlyFunc 		getSettingIsReadOnly;

	BSGetExistingProfilesFunc	getExistingProfiles;
	BSDeleteProfileFunc			deleteProfile;
};

struct _BSPlugin
{
	char *				name;
	char *				shortDesc;		// in current locale
	char *				longDesc;		// in current locale
	char *				hints;
	char *				category;		// simple name
	char *				filename;		// filename of the so
	
	BSStringList		loadAfter;
	BSStringList		loadBefore;
	BSStringList		provides;
	BSStringList		requires;
	BSSettingList		settings;
	BSGroupList			groups;
	void *				privatePtr;
	BSContext *			context;
};

typedef enum _BSSettingType
{
 	TypeBool,
	TypeInt,
	TypeFloat,
	TypeString,
	TypeAction,
	TypeColor,
	TypeMatch,
	TypeList,
	TypeNum
} BSSettingType;

struct _BSSubGroup
{
	char *				name;		//in current locale
	BSSettingList		settings;	//list of BerylSetting
};

struct _BSGroup
{
	char *				name;		//in current locale
	BSSubGroupList	subGroups;	//list of BerylSettingsSubGroup
};

typedef enum _BSConflictType
{
	ConflictKey,
	ConflictButton,
	ConflictEdge,
	ConflctAny,
} BSConflictType;

typedef struct _BSSettingConflict
{
	BSSettingList		settings;	// settings that conflict over the binding
	BSConflictType		type;		// type of the conflict, note that a setting may show up again in another
									// list for a different type
} BSConflict;

union _BSSettingInfo;

typedef struct _BSSettingIntInfo
{
	int		min;
	int		max;
} BSSettingIntInfo;

typedef struct _BSSettingFloatInfo
{
	float	min;
	float	max;
	float	precision;
} BSSettingFloatInfo;

typedef struct _BSSettingStringInfo
{
	BSStringList	allowedValues; //list_of(char *) in current locale
} BSSettingStringInfo;

typedef struct _BSSettingActionInfo
{
	Bool	key;
	Bool	button;
	Bool	bell;
	Bool	edge;
} BSSettingActionInfo;

typedef struct _BSSettingActionArrayInfo
{
	Bool	array[4];
} BSSettingActionArrayInfo;

typedef struct _BSSettingListInfo
{
	BSSettingType			listType;
	union _BSSettingInfo *	listInfo;
} BSSettingListInfo;

typedef union _BSSettingInfo
{
	BSSettingIntInfo			forInt;
	BSSettingFloatInfo			forFloat;
	BSSettingStringInfo			forString;
	BSSettingActionInfo			forAction;
	BSSettingActionArrayInfo	forActionAsArray;
	BSSettingListInfo			forList;
} BSSettingInfo;

typedef struct _BSSettingColorValueColor
{
	unsigned short	red;
	unsigned short	green;
	unsigned short	blue;
	unsigned short	alpha;
} BSSettingColorValueColor;

typedef struct _BSSettingColorValueArray
{
	unsigned short	array[4];
} BSSettingColorValueArray;

typedef union _BSSettingColorValue
{
	BSSettingColorValueColor	color;
	BSSettingColorValueArray	array;
} BSSettingColorValue;


typedef struct _BSSettingActionValue
{
	int				button;
	unsigned int	buttonModMask;
	int				keysym;
	unsigned int	keyModMask;
	Bool			onBell;
	int				edgeMask;
	int             edgeButton;
} BSSettingActionValue;

typedef union _BSSettingValueUnion
{
	Bool					asBool;
	int						asInt;
	float					asFloat;
	char *					asString;
	char *					asMatch;
	BSSettingActionValue	asAction;
	BSSettingColorValue		asColor;
	BSSettingValueList		asList;        // list_of(BerylSettingValue *)
} BSSettingValueUnion;

struct _BSSettingValue
{
	BSSettingValueUnion		value;
	BSSetting *				parent;
	Bool					isListChild;
};

struct _BSSetting
{
	char * 				name;
	char *				shortDesc;        // in current locale
	char *				longDesc;        // in current locale

	BSSettingType		type;
	Bool				isScreen;        // support the 'screen/display' thing
	unsigned int		screenNum;

	BSSettingInfo		info;
	char *				group;		// in current locale
	char *				subGroup;		// in current locale
	char * 				hints;	// in current locale

	BSSettingValue		defaultValue;
	BSSettingValue *	value; // = &default_value if isDefault == TRUE
	Bool				isDefault;

	BSPlugin *			parent;
	void * 				privatePtr;
};

struct _BSPluginCategory
{
	const char *		name;
	const char *		shortDesc;
	const char *		longDesc;
	BSStringList		plugins;
};


BSContext * bsContextNew(void);
void bsContextDestroy(BSContext * context);

BSPlugin * bsFindPlugin(BSContext *context, char * name);

BSSetting * bsFindSetting(BSPlugin *plugin, char * name,
						  Bool isScreen, unsigned int screenNum);

Bool bsSetBackend(BSContext *context, char *name);

void bsFreeContext(BSContext *context);
void bsFreePlugin(BSPlugin *plugin);
void bsFreeSetting(BSSetting *setting);
void bsFreeGroup(BSGroup *group);
void bsFreeSubGroup(BSSubGroup *subGroup);
void bsFreeSettingValue(BSSettingValue *value);
#define bsFreeString(val) free(val)

Bool bsSetInt(BSSetting * setting, int data);
Bool bsSetFloat(BSSetting * setting, float data);
Bool bsSetBool(BSSetting * setting, Bool data);
Bool bsSetString(BSSetting * setting, const char * data);
Bool bsSetColor(BSSetting * setting, BSSettingColorValue data);
Bool bsSetMatch(BSSetting * setting, const char * data);
Bool bsSetAction(BSSetting * setting, BSSettingActionValue data);
Bool bsSetList(BSSetting * setting, BSSettingValueList data);

Bool bsGetInt(BSSetting * setting, int * data);
Bool bsGetFloat(BSSetting * setting, float * data);
Bool bsGetBool(BSSetting * setting, Bool * data);
Bool bsGetString(BSSetting * setting, char ** data);
Bool bsGetColor(BSSetting * setting, BSSettingColorValue * data);
Bool bsGetMatch(BSSetting * setting, char ** data); 
Bool bsGetAction(BSSetting * setting, BSSettingActionValue * data);
Bool bsGetList(BSSetting * setting, BSSettingValueList*data);

BSSettingValueList bsGetValueListFromStringList(BSStringList list, BSSetting *parent);
BSStringList bsGetStringListFromValueList(BSSettingValueList list);

char ** bsGetStringArrayFromList(BSStringList list, int *num);
BSStringList bsGetListFromStringArray(char ** array, int num);

char ** bsGetStringArrayFromValueList(BSSettingValueList list, int *num);
char ** bsGetMatchArrayFromValueList(BSSettingValueList list, int *num);
int * bsGetIntArrayFromValueList(BSSettingValueList list, int *num);
float * bsGetFloatArrayFromValueList(BSSettingValueList list, int *num);
Bool * bsGetBoolArrayFromValueList(BSSettingValueList list, int *num);
BSSettingColorValue * bsGetColorArrayFromValueList(BSSettingValueList list, int *num);
BSSettingActionValue * bsGetActionArrayFromValueList(BSSettingValueList list, int *num);

BSSettingValueList bsGetValueListFromStringArray(char ** array, int num, BSSetting *parent);
BSSettingValueList bsGetValueListFromMatchArray(char ** array, int num, BSSetting *parent);
BSSettingValueList bsGetValueListFromIntArray(int * array, int num, BSSetting *parent);
BSSettingValueList bsGetValueListFromFloatArray(float * array, int num, BSSetting *parent);
BSSettingValueList bsGetValueListFromBoolArray(Bool * array, int num, BSSetting *parent);
BSSettingValueList bsGetValueListFromColorArray(BSSettingColorValue * array, int num, BSSetting *parent);
BSSettingValueList bsGetValueListFromActionArray(BSSettingActionValue * array, int num, BSSetting *parent);

BSPluginList bsGetActivePluginList(BSContext *context);
BSStringList bsGetSortedPluginStringList(BSContext *context);

Bool bsGetIntegrationEnabled(BSContext *context);
char * bsGetProfile(BSContext *context);

//utility functions
unsigned int bsGetModsAndEndptr(char * src, char ** ret);
char * bsModsToString(unsigned int mods);

void bsProcessEvents(BSContext *context);

#endif
