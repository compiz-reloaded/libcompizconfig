#ifndef BSETTINGS_PRIVATE_H
#define BSETTINGS_PRIVATE_H

#include <bsettings.h>

void bsLoadPlugins(BSContext * context);
void collateGroups(BSPlugin * plugin);

void bsCheckFileWatches(void);

typedef enum {
	OptionProfile,
	OptionBackend,
	OptionIntegration
} ConfigOption;

Bool bsReadConfig(ConfigOption option, char** value);
Bool bsWriteConfig(ConfigOption option, char* value);

#endif
