#ifndef _BSETTINGS_H
#define _BSETTINGS_H

#ifndef Bool
#define Bool int
#endif

#ifndef TRUE
#define TRUE ~0
#endif

#ifndef FALSE
#define FALSE 0
#endif

#define NEW(a,b) \
	a * b = malloc(sizeof(a)); memset((b),0,sizeof(a))

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
typedef struct _BSPluginConflict	BSPluginConflict;
typedef struct _BSActionConflict	BSActionConflict;

BSLIST_HDR(Plugin,BSPlugin)
BSLIST_HDR(Setting,BSSetting)
BSLIST_HDR(String,char)
BSLIST_HDR(Group,BSGroup)
BSLIST_HDR(SubGroup,BSSubGroup)
BSLIST_HDR(SettingValue,BSSettingValue)
BSLIST_HDR(PluginConflict,BSPluginConflict)
BSLIST_HDR(ActionConflict,BSActionConflict)

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

	unsigned int        configWatchId;
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
	BSStringList		requiresPlugin;
        BSStringList		providesFeature;
	BSStringList		requiresFeature;
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

typedef enum _BSPluginConflictType
{
    // produced on plugin activation
    ConflictRequiresPlugin,
    ConflictRequiresFeature,
    ConflictSameFeature,
    // produced on plugin deactivation
    ConflictFeatureNeeded,
    ConflictPluginNeeded,
    ConflictPluginError,
} BSPluginConflictType;

struct _BSPluginConflict
{
    char *                  value;
    BSPluginConflictType    type;
    BSPluginList            plugins;
};

typedef enum _BSActionConflictType
{
    ConflictKey,
    ConflictButton,
    ConflictEdge
} BSActionConflictType;

struct _BSActionConflict
{
    BSActionConflictType    type;
    BSSettingList           settings;
};

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
BSBackendVTable *getBackendInfo (void);

BSPlugin * bsFindPlugin(BSContext *context, char * name);

BSSetting * bsFindSetting(BSPlugin *plugin, char * name,
						  Bool isScreen, unsigned int screenNum);

Bool bsPluginIsActive(BSContext * context, char * name);

void bsFreeContext(BSContext *context);
void bsFreePlugin(BSPlugin *plugin);
void bsFreeSetting(BSSetting *setting);
void bsFreeGroup(BSGroup *group);
void bsFreeSubGroup(BSSubGroup *subGroup);
void bsFreeSettingValue(BSSettingValue *value);
void bsFreePluginConflict(BSPluginConflict *value);
void bsFreeActionConflict(BSActionConflict *value);
#define bsFreeString(val) free(val)

Bool bsSetInt(BSSetting * setting, int data);
Bool bsSetFloat(BSSetting * setting, float data);
Bool bsSetBool(BSSetting * setting, Bool data);
Bool bsSetString(BSSetting * setting, const char * data);
Bool bsSetColor(BSSetting * setting, BSSettingColorValue data);
Bool bsSetMatch(BSSetting * setting, const char * data);
Bool bsSetAction(BSSetting * setting, BSSettingActionValue data);
Bool bsSetList(BSSetting * setting, BSSettingValueList data);
Bool bsSetValue(BSSetting * setting, BSSettingValue *data);

Bool bsIsEqualColor(BSSettingColorValue c1, BSSettingColorValue c2);
Bool bsIsEqualAction(BSSettingActionValue c1, BSSettingActionValue c2);

Bool bsGetInt(BSSetting * setting, int * data);
Bool bsGetFloat(BSSetting * setting, float * data);
Bool bsGetBool(BSSetting * setting, Bool * data);
Bool bsGetString(BSSetting * setting, char ** data);
Bool bsGetColor(BSSetting * setting, BSSettingColorValue * data);
Bool bsGetMatch(BSSetting * setting, char ** data); 
Bool bsGetAction(BSSetting * setting, BSSettingActionValue * data);
Bool bsGetList(BSSetting * setting, BSSettingValueList * data);

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

Bool bsSetBackend(BSContext *context, char *name);
void bsSetIntegrationEnabled(BSContext *context, Bool value);
void bsSetProfile(BSContext *context, char *name);

char * bsGetProfile(BSContext *context);
Bool bsGetIntegrationEnabled(BSContext *context);

Bool bsPluginSetActive(BSPlugin *plugin, Bool value);

/* functions parsing/creating an action string -
   the returned strings must be free'd after usage! */

char * bsKeyBindingToString (BSSettingActionValue *action);
char * bsButtonBindingToString (BSSettingActionValue *action);
char * bsEdgeToString (BSSettingActionValue *action);
char * bsColorToString (BSSettingColorValue *color);

Bool bsStringToKeyBinding (const char *binding, BSSettingActionValue *action);
Bool bsStringToButtonBinding (const char *binding, BSSettingActionValue *action);
void bsStringToEdge (const char *edge, BSSettingActionValue *action);
Bool bsStringToColor (const char *value, BSSettingColorValue *color);

void bsProcessEvents(BSContext *context);

void bsReadSettings(BSContext *context);
void bsWriteSettings(BSContext *context);
void bsWriteChangedSettings(BSContext *context);
void bsResetToDefault (BSSetting * setting);

/* File import / export */
Bool bsExportToFile (BSContext * context, const char * fileName);
Bool bsImportFromFile (BSContext * context, const char * fileName, Bool overwrite);

/* File watch stuff */

typedef void (*FileWatchCallbackProc) (unsigned int watchId, void *closure);
unsigned int bsAddFileWatch (const char *fileName, Bool enable, FileWatchCallbackProc callback, void *closure);
void bsRemoveFileWatch (unsigned int watchId);
void bsDisableFileWatch (unsigned int watchId);
void bsEnableFileWatch (unsigned int watchId);

/* INI file stuff */

typedef struct _dictionary_ {
	/** Number of entries in dictionary */
	int n;
	/** Storage size */
	int size;
	/** List of string values */
	char **val;
	/** List of string keys */
	char **key ;
	/** List of hash values for keys */
	unsigned *hash;
} IniDictionary;

IniDictionary * bsIniNew (void);
IniDictionary * bsIniOpen (const char * fileName);
void bsIniClose (IniDictionary * dictionary);
void bsIniSave (IniDictionary * dictionary, const char * fileName);
Bool bsIniGetString (IniDictionary * dictionary, const char * section,
		     const char * entry, char ** value);
Bool bsIniGetInt (IniDictionary * dictionary, const char * section,
		  const char * entry, int * value);
Bool bsIniGetFloat (IniDictionary * dictionary, const char * section,
		    const char * entry, float * value);
Bool bsIniGetBool (IniDictionary * dictionary, const char * section,
		   const char * entry, Bool * value);
Bool bsIniGetColor (IniDictionary * dictionary, const char * section,
		    const char * entry, BSSettingColorValue * value);
Bool bsIniGetAction (IniDictionary * dictionary, const char * section,
		     const char * entry, BSSettingActionValue * value);
Bool bsIniGetList (IniDictionary * dictionary, const char * section,
		   const char * entry, BSSettingValueList * value, BSSetting *parent);
void bsIniSetString (IniDictionary * dictionary, const char * section,
		     const char * entry, char * value);
void bsIniSetInt (IniDictionary * dictionary, const char * section,
		  const char * entry, int value);
void bsIniSetFloat (IniDictionary * dictionary, const char * section,
    		    const char * entry, float value);
void bsIniSetBool (IniDictionary * dictionary, const char * section,
		   const char * entry, Bool value);
void bsIniSetColor (IniDictionary * dictionary, const char * section,
		    const char * entry, BSSettingColorValue value);
void bsIniSetAction (IniDictionary * dictionary, const char * section,
		     const char * entry, BSSettingActionValue value);
void bsIniSetList (IniDictionary * dictionary, const char * section,
		   const char * entry, BSSettingValueList value, BSSettingType listType);

void bsIniRemoveEntry (IniDictionary * dictionary, const char * section,
					   const char * entry);

/* plugin conflict handling */
BSPluginConflictList bsCanEnablePlugin (BSContext * context, BSPlugin * plugin);
BSPluginConflictList bsCanDisablePlugin (BSContext * context, BSPlugin * plugin);

BSActionConflictList bsCanSetAction (BSContext * context, BSSettingActionValue action);

BSStringList bsGetExistingProfiles (BSContext * context);
void bsDeleteProfile (BSContext * context, char *name);

#endif
