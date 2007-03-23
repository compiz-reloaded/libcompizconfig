#ifndef __OPTION_H__
#define __OPTION_H__

#include <bsettings.h>

char *
keyBindingToString (BSSettingActionValue *action);

Bool
stringToKeyBinding (const char           *binding,
		    BSSettingActionValue *action);

char *
buttonBindingToString (BSSettingActionValue *action);

Bool
stringToButtonBinding (const char           *binding,
		       BSSettingActionValue *action);

char *
edgeToString (unsigned int edge);

void stringToEdge (const char           *edge,
		   BSSettingActionValue *action);

Bool
stringToColor (const char     *color,
	       unsigned short *rgba);

char *
colorToString (unsigned short *rgba);

#endif
