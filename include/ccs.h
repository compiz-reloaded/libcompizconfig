/*
 * Compiz configuration system library
 * 
 * Copyright (C) 2007  Dennis Kasprzyk <onestone@beryl-project.org>
 * Copyright (C) 2007  Danny Baumann <maniac@beryl-project.org>
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
 

#ifndef _CSS_H
#define _CSS_H

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

#define CCSLIST_HDR(type,dtype)		\
typedef struct _CCS##type##List *	CCS##type##List;\
struct _CCS##type##List	\
{								\
	dtype   * data;			\
	CCS##type##List next;		\
}; \
CCS##type##List ccs##type##ListAppend (CCS##type##List list, dtype *data); \
CCS##type##List ccs##type##ListPrepend (CCS##type##List list, dtype *data); \
CCS##type##List ccs##type##ListInsert (CCS##type##List list, dtype *data, int position); \
CCS##type##List ccs##type##ListInsertBefore (CCS##type##List list, CCS##type##List sibling, dtype *data); \
unsigned int ccs##type##ListLength (CCS##type##List list); \
CCS##type##List ccs##type##ListFind (CCS##type##List list, dtype *data); \
CCS##type##List ccs##type##ListGetItem (CCS##type##List list, unsigned int index); \
CCS##type##List ccs##type##ListRemove (CCS##type##List list, dtype *data, Bool freeObj); \
CCS##type##List ccs##type##ListFree (CCS##type##List list, Bool freeObj);

typedef struct _CCSContext			CCSContext;
typedef struct _CCSPlugin			CCSPlugin;
typedef struct _CCSSetting			CCSSetting;
typedef struct _CCSGroup			CCSGroup;
typedef struct _CCSSubGroup			CCSSubGroup;
typedef struct _CCSBackend			CCSBackend;
typedef struct _CCSBackendVTable 	CCSBackendVTable;
typedef struct _CCSPluginCategory	CCSPluginCategory;
typedef struct _CCSSettingValue		CCSSettingValue;
typedef struct _CCSPluginConflict	CCSPluginConflict;
typedef struct _CCSActionConflict	CCSActionConflict;
typedef struct _CCSBackendInfo		CCSBackendInfo;
typedef struct _CCSIntDesc			CCSIntDesc;

CCSLIST_HDR(Plugin,CCSPlugin)
CCSLIST_HDR(Setting,CCSSetting)
CCSLIST_HDR(String,char)
CCSLIST_HDR(Group,CCSGroup)
CCSLIST_HDR(SubGroup,CCSSubGroup)
CCSLIST_HDR(SettingValue,CCSSettingValue)
CCSLIST_HDR(PluginConflict,CCSPluginConflict)
CCSLIST_HDR(ActionConflict,CCSActionConflict)
CCSLIST_HDR(BackendInfo,CCSBackendInfo)
CCSLIST_HDR(IntDesc,CCSIntDesc)
		
struct _CCSContext
{
	CCSPluginList		plugins;
	CCSPluginCategory *	categories;
	void *				privatePtr;

	CCSBackend *			backend;

	char *				profile;
	Bool				deIntegration;

	CCSSettingList		changedSettings;
	Bool 				pluginsChanged;

	unsigned int        configWatchId;
	
	unsigned int *		screens;
	unsigned int        numScreens;
};

struct _CCSBackend
{
	void *				dlhand;
	CCSBackendVTable *	vTable;
};

typedef CCSBackendVTable *(*BackendGetInfoProc) (void);

typedef void (*CCSExecuteEventsFunc) (unsigned int flags);

typedef Bool (*CCSInitBackendFunc) (CCSContext * context);
typedef Bool (*CCSFiniBackendFunc) (CCSContext * context);

typedef Bool (*CCSContextReadInitFunc) (CCSContext * context);
typedef void (*CCSContextReadSettingFunc)
					(CCSContext * context, CCSSetting * setting);
typedef void (*CCSContextReadDoneFunc) (CCSContext * context);

typedef Bool (*CCSContextWriteInitFunc) (CCSContext * context);
typedef void (*CCSContextWriteSettingFunc)
					(CCSContext * context, CCSSetting * setting);
typedef void (*CCSContextWriteDoneFunc) (CCSContext * context);

typedef Bool (*CCSGetIsIntegratedFunc) (CCSSetting * setting);
typedef Bool (*CCSGetIsReadOnlyFunc) (CCSSetting * setting);

typedef CCSStringList (*CCSGetExistingProfilesFunc) (CCSContext * context);
typedef Bool (*CCSDeleteProfileFunc) (CCSContext * context, char * name);

struct _CCSBackendVTable
{
	char *				name;
	char *				shortDesc;
	char *				longDesc;
	Bool				integrationSupport;
	Bool				profileSupport;

	CCSExecuteEventsFunc			executeEvents; // something like a event loop call for the backend
	// so it can check for file changes (gconf changes in the gconf backend)
	// no need for reload settings signals anymore

	CCSInitBackendFunc			backendInit;
	CCSFiniBackendFunc			backendFini;

   	CCSContextReadInitFunc		readInit;
   	CCSContextReadSettingFunc	readSetting;
   	CCSContextReadDoneFunc		readDone;

   	CCSContextWriteInitFunc		writeInit;
   	CCSContextWriteSettingFunc	writeSetting;
   	CCSContextWriteDoneFunc		writeDone;


	CCSGetIsIntegratedFunc 		getSettingIsIntegrated;
	CCSGetIsReadOnlyFunc 		getSettingIsReadOnly;

	CCSGetExistingProfilesFunc	getExistingProfiles;
	CCSDeleteProfileFunc			deleteProfile;
};

struct _CCSBackendInfo
{
	char *				name;
	char *				shortDesc;
	char *				longDesc;
	Bool				integrationSupport;
	Bool				profileSupport;
};

struct _CCSPlugin
{
	char *				name;
	char *				shortDesc;		// in current locale
	char *				longDesc;		// in current locale
	char *				hints;
	char *				category;		// simple name
	
	CCSStringList		loadAfter;
	CCSStringList		loadBefore;
	CCSStringList		requiresPlugin;
	CCSStringList		conflictPlugin;
	CCSStringList		conflictFeature;
	CCSStringList		providesFeature;
	CCSStringList		requiresFeature;

	void *				privatePtr;
	CCSContext *		context;
	
	void *              ccsPrivate;
};

typedef enum _CCSSettingType
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
} CCSSettingType;

struct _CCSSubGroup
{
	char *				name;		//in current locale
	CCSSettingList		settings;	//list of BerylSetting
};

struct _CCSGroup
{
	char *				name;		//in current locale
	CCSSubGroupList	subGroups;	//list of BerylSettingsSubGroup
};

typedef enum _CCSPluginConflictType
{
    // produced on plugin activation
    ConflictRequiresPlugin,
    ConflictRequiresFeature,
    ConflictFeature,
	ConflictPlugin,
    // produced on plugin deactivation
    ConflictFeatureNeeded,
    ConflictPluginNeeded,
    ConflictPluginError,
} CCSPluginConflictType;

struct _CCSPluginConflict
{
    char *                  value;
    CCSPluginConflictType    type;
    CCSPluginList            plugins;
};

typedef enum _CCSActionConflictType
{
    ConflictKey,
    ConflictButton,
    ConflictEdge
} CCSActionConflictType;

struct _CCSActionConflict
{
    CCSActionConflictType    type;
    CCSSettingList           settings;
};

union _CCSSettingInfo;

struct _CCSIntDesc
{
	int		value;
	char    *name;
};

typedef struct _CCSSettingIntInfo
{
	int				min;
	int				max;
	CCSIntDescList	desc;
} CCSSettingIntInfo;

typedef struct _CCSSettingFloatInfo
{
	float	min;
	float	max;
	float	precision;
} CCSSettingFloatInfo;

typedef struct _CCSSettingActionInfo
{
	Bool	key;
	Bool	button;
	Bool	bell;
	Bool	edge;
} CCSSettingActionInfo;

typedef struct _CSSettingActionArrayInfo
{
	Bool	array[4];
} CCSSettingActionArrayInfo;

typedef struct _CCSSettingListInfo
{
	CCSSettingType			listType;
	union _CCSSettingInfo *	listInfo;
} CCSSettingListInfo;

typedef union _CCSSettingInfo
{
	CCSSettingIntInfo			forInt;
	CCSSettingFloatInfo			forFloat;
	CCSSettingActionInfo			forAction;
	CCSSettingActionArrayInfo	forActionAsArray;
	CCSSettingListInfo			forList;
} CCSSettingInfo;

typedef struct _CCSSettingColorValueColor
{
	unsigned short	red;
	unsigned short	green;
	unsigned short	blue;
	unsigned short	alpha;
} CCSSettingColorValueColor;

typedef struct _CCSSettingColorValueArray
{
	unsigned short	array[4];
} CCSSettingColorValueArray;

typedef union _CCSSettingColorValue
{
	CCSSettingColorValueColor	color;
	CCSSettingColorValueArray	array;
} CCSSettingColorValue;


typedef struct _CCSSettingActionValue
{
	int				button;
	unsigned int	buttonModMask;
	int				keysym;
	unsigned int	keyModMask;
	Bool			onBell;
	int				edgeMask;
	int             edgeButton;
} CCSSettingActionValue;

typedef union _CCSSettingValueUnion
{
	Bool					asBool;
	int						asInt;
	float					asFloat;
	char *					asString;
	char *					asMatch;
	CCSSettingActionValue	asAction;
	CCSSettingColorValue		asColor;
	CCSSettingValueList		asList;        
} CCSSettingValueUnion;

struct _CCSSettingValue
{
	CCSSettingValueUnion		value;
	CCSSetting *				parent;
	Bool					isListChild;
};

struct _CCSSetting
{
	char * 				name;
	char *				shortDesc;        // in current locale
	char *				longDesc;        // in current locale

	CCSSettingType		type;
	Bool				isScreen;        // support the 'screen/display' thing
	unsigned int		screenNum;

	CCSSettingInfo		info;
	char *				group;		// in current locale
	char *				subGroup;		// in current locale
	char * 				hints;	// in current locale

	CCSSettingValue		defaultValue;
	CCSSettingValue *	value; // = &default_value if isDefault == TRUE
	Bool				isDefault;

	CCSPlugin *			parent;
	void * 				privatePtr;
};

struct _CCSPluginCategory
{
	const char *		name;
	const char *		shortDesc;
	const char *		longDesc;
	CCSStringList		plugins;
};


CCSContext * ccsContextNew(unsigned int *screens, unsigned int numScreens);
void ccsContextDestroy(CCSContext * context);
CCSBackendVTable *getBackendInfo (void);

CCSPlugin * ccsFindPlugin(CCSContext *context, char * name);

CCSSetting * ccsFindSetting(CCSPlugin *plugin, char * name,
						  Bool isScreen, unsigned int screenNum);

Bool ccsPluginIsActive(CCSContext * context, char * name);

void ccsFreeContext(CCSContext *context);
void ccsFreePlugin(CCSPlugin *plugin);
void ccsFreeSetting(CCSSetting *setting);
void ccsFreeGroup(CCSGroup *group);
void ccsFreeSubGroup(CCSSubGroup *subGroup);
void ccsFreeSettingValue(CCSSettingValue *value);
void ccsFreePluginConflict(CCSPluginConflict *value);
void ccsFreeActionConflict(CCSActionConflict *value);
void ccsFreeBackendInfo(CCSBackendInfo *value);
void ccsFreeIntDesc(CCSIntDesc *value);
#define ccsFreeString(val) free(val)

Bool ccsSetInt(CCSSetting * setting, int data);
Bool ccsSetFloat(CCSSetting * setting, float data);
Bool ccsSetBool(CCSSetting * setting, Bool data);
Bool ccsSetString(CCSSetting * setting, const char * data);
Bool ccsSetColor(CCSSetting * setting, CCSSettingColorValue data);
Bool ccsSetMatch(CCSSetting * setting, const char * data);
Bool ccsSetAction(CCSSetting * setting, CCSSettingActionValue data);
Bool ccsSetList(CCSSetting * setting, CCSSettingValueList data);
Bool ccsSetValue(CCSSetting * setting, CCSSettingValue *data);

Bool ccsIsEqualColor(CCSSettingColorValue c1, CCSSettingColorValue c2);
Bool ccsIsEqualAction(CCSSettingActionValue c1, CCSSettingActionValue c2);

Bool ccsGetInt(CCSSetting * setting, int * data);
Bool ccsGetFloat(CCSSetting * setting, float * data);
Bool ccsGetBool(CCSSetting * setting, Bool * data);
Bool ccsGetString(CCSSetting * setting, char ** data);
Bool ccsGetColor(CCSSetting * setting, CCSSettingColorValue * data);
Bool ccsGetMatch(CCSSetting * setting, char ** data); 
Bool ccsGetAction(CCSSetting * setting, CCSSettingActionValue * data);
Bool ccsGetList(CCSSetting * setting, CCSSettingValueList * data);

CCSSettingList ccsGetPluginSettings(CCSPlugin *plugin);
CCSGroupList ccsGetPluginGroups(CCSPlugin *plugin);

CCSSettingValueList ccsGetValueListFromStringList(CCSStringList list, CCSSetting *parent);
CCSStringList ccsGetStringListFromValueList(CCSSettingValueList list);

char ** ccsGetStringArrayFromList(CCSStringList list, int *num);
CCSStringList ccsGetListFromStringArray(char ** array, int num);

char ** ccsGetStringArrayFromValueList(CCSSettingValueList list, int *num);
char ** ccsGetMatchArrayFromValueList(CCSSettingValueList list, int *num);
int * ccsGetIntArrayFromValueList(CCSSettingValueList list, int *num);
float * ccsGetFloatArrayFromValueList(CCSSettingValueList list, int *num);
Bool * ccsGetBoolArrayFromValueList(CCSSettingValueList list, int *num);
CCSSettingColorValue * ccsGetColorArrayFromValueList(CCSSettingValueList list, int *num);
CCSSettingActionValue * ccsGetActionArrayFromValueList(CCSSettingValueList list, int *num);

CCSSettingValueList ccsGetValueListFromStringArray(char ** array, int num, CCSSetting *parent);
CCSSettingValueList ccsGetValueListFromMatchArray(char ** array, int num, CCSSetting *parent);
CCSSettingValueList ccsGetValueListFromIntArray(int * array, int num, CCSSetting *parent);
CCSSettingValueList ccsGetValueListFromFloatArray(float * array, int num, CCSSetting *parent);
CCSSettingValueList ccsGetValueListFromBoolArray(Bool * array, int num, CCSSetting *parent);
CCSSettingValueList ccsGetValueListFromColorArray(CCSSettingColorValue * array, int num, CCSSetting *parent);
CCSSettingValueList ccsGetValueListFromActionArray(CCSSettingActionValue * array, int num, CCSSetting *parent);

CCSPluginList ccsGetActivePluginList(CCSContext *context);
CCSStringList ccsGetSortedPluginStringList(CCSContext *context);

Bool ccsSetBackend(CCSContext *context, char *name);
void ccsSetIntegrationEnabled(CCSContext *context, Bool value);
void ccsSetProfile(CCSContext *context, char *name);

char * ccsGetProfile(CCSContext *context);
Bool ccsGetIntegrationEnabled(CCSContext *context);

Bool ccsPluginSetActive(CCSPlugin *plugin, Bool value);

/* functions parsing/creating an action string -
   the returned strings must be free'd after usage! */

char * ccsModifiersToString (unsigned int modMask);
char * ccsKeyBindingToString (CCSSettingActionValue *action);
char * ccsButtonBindingToString (CCSSettingActionValue *action);
CCSStringList ccsEdgesToStringList (CCSSettingActionValue *action);
char * ccsColorToString (CCSSettingColorValue *color);

unsigned int ccsStringToModifiers (const char *binding);
Bool ccsStringToKeyBinding (const char *binding, CCSSettingActionValue *action);
Bool ccsStringToButtonBinding (const char *binding, CCSSettingActionValue *action);
void ccsStringListToEdges (CCSStringList edges, CCSSettingActionValue *action);
Bool ccsStringToColor (const char *value, CCSSettingColorValue *color);

/* flag values for ccsProcessEvents */
#define ProcessEventsNoGlibMainLoopMask (1 << 0)

void ccsProcessEvents(CCSContext *context, unsigned int flags);

void ccsReadSettings(CCSContext *context);
void ccsReadPluginSettings(CCSPlugin *plugin);
void ccsWriteSettings(CCSContext *context);
void ccsWriteChangedSettings(CCSContext *context);
void ccsResetToDefault (CCSSetting * setting);

/* File import / export */
Bool ccsExportToFile (CCSContext * context, const char * fileName);
Bool ccsImportFromFile (CCSContext * context, const char * fileName, Bool overwrite);

/* File watch stuff */

typedef void (*FileWatchCallbackProc) (unsigned int watchId, void *closure);
unsigned int ccsAddFileWatch (const char *fileName, Bool enable, FileWatchCallbackProc callback, void *closure);
void ccsRemoveFileWatch (unsigned int watchId);
void ccsDisableFileWatch (unsigned int watchId);
void ccsEnableFileWatch (unsigned int watchId);

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

IniDictionary * ccsIniNew (void);
IniDictionary * ccsIniOpen (const char * fileName);
void ccsIniClose (IniDictionary * dictionary);
void ccsIniSave (IniDictionary * dictionary, const char * fileName);
Bool ccsIniGetString (IniDictionary * dictionary, const char * section,
		     const char * entry, char ** value);
Bool ccsIniGetInt (IniDictionary * dictionary, const char * section,
		  const char * entry, int * value);
Bool ccsIniGetFloat (IniDictionary * dictionary, const char * section,
		    const char * entry, float * value);
Bool ccsIniGetBool (IniDictionary * dictionary, const char * section,
		   const char * entry, Bool * value);
Bool ccsIniGetColor (IniDictionary * dictionary, const char * section,
		    const char * entry, CCSSettingColorValue * value);
Bool ccsIniGetAction (IniDictionary * dictionary, const char * section,
		     const char * entry, CCSSettingActionValue * value);
Bool ccsIniGetList (IniDictionary * dictionary, const char * section,
		   const char * entry, CCSSettingValueList * value, CCSSetting *parent);
void ccsIniSetString (IniDictionary * dictionary, const char * section,
		     const char * entry, char * value);
void ccsIniSetInt (IniDictionary * dictionary, const char * section,
		  const char * entry, int value);
void ccsIniSetFloat (IniDictionary * dictionary, const char * section,
    		    const char * entry, float value);
void ccsIniSetBool (IniDictionary * dictionary, const char * section,
		   const char * entry, Bool value);
void ccsIniSetColor (IniDictionary * dictionary, const char * section,
		    const char * entry, CCSSettingColorValue value);
void ccsIniSetAction (IniDictionary * dictionary, const char * section,
		     const char * entry, CCSSettingActionValue value);
void ccsIniSetList (IniDictionary * dictionary, const char * section,
		   const char * entry, CCSSettingValueList value, CCSSettingType listType);

void ccsIniRemoveEntry (IniDictionary * dictionary, const char * section,
					   const char * entry);

/* plugin conflict handling */
CCSPluginConflictList ccsCanEnablePlugin (CCSContext * context, CCSPlugin * plugin);
CCSPluginConflictList ccsCanDisablePlugin (CCSContext * context, CCSPlugin * plugin);

CCSActionConflictList ccsCanSetAction (CCSContext * context, CCSSettingActionValue action);

CCSStringList ccsGetExistingProfiles (CCSContext * context);
void ccsDeleteProfile (CCSContext * context, char *name);

CCSBackendInfoList ccsGetExistingBackends (void);

Bool ccsSettingIsIntegrated (CCSSetting *setting);
Bool ccsSettingIsReadOnly (CCSSetting *setting);

#endif
