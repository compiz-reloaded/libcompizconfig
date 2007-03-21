#ifndef BSETTINGS_PRIVATE_H
#define BSETTINGS_PRIVATE_H

#include <bsettings.h>

void bsLoadPlugin(BSContext * context, char * filename);
void collateGroups(BSPlugin * plugin);

typedef enum {
	OptionProfile,
	OptionBackend,
	OptionIntegration
} ConfigOption;

Bool bsReadConfig(ConfigOption option, char** value);
Bool bsWriteConfig(ConfigOption option, char* value);

#endif
