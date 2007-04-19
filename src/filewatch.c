#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>

#include <sys/inotify.h>
#include <fcntl.h>

#include <bsettings.h>
#include "bsettings-private.h"

typedef struct _FilewatchData
{
    char *fileName;
    int watchDesc;
    unsigned int watchId;
    FileWatchCallbackProc callback;
    void *closure;
} FilewatchData;

static FilewatchData *fwData = NULL;
static int fwDataSize = 0;
static int inotifyFd = 0;

static inline int findDataIndexById(unsigned int watchId)
{
    int i, index = -1;

    for (i = 0; i < fwDataSize; i++)
	if (fwData[i].watchId == watchId)
	{
	    index = i;
	    break;
	}

    return index;
}

void bsCheckFileWatches(void)
{
    char buf[256 * (sizeof (struct inotify_event) + 16)];
    struct  inotify_event *event;
    int	len, i = 0, j;

    if (!inotifyFd)
	return;

    len = read (inotifyFd, buf, sizeof (buf));

    if (len < 0)
	return;

    while (i < len)
    {
	event = (struct inotify_event *) &buf[i];

	for (j = 0; j < fwDataSize; j++)
	    if ((fwData[j].watchDesc == event->wd) && fwData[j].callback)
			(*fwData[j].callback) (fwData[j].watchId, fwData[j].closure);

	    i += sizeof (*event) + event->len;
    }
}

unsigned int bsAddFileWatch (const char *fileName, 
			     Bool enable,
			     FileWatchCallbackProc callback,
			     void *closure)
{
    unsigned int i, maxWatchId = 0;

    if (!inotifyFd)
    {
	inotifyFd = inotify_init ();
	fcntl (inotifyFd, F_SETFL, O_NONBLOCK);
    }


    fwData = realloc (fwData, (fwDataSize + 1) * sizeof(FilewatchData));

    fwData[fwDataSize].fileName  = strdup (fileName);

    if (enable)
	fwData[fwDataSize].watchDesc = inotify_add_watch (inotifyFd, fileName,
							  IN_MODIFY | IN_MOVE |
							  IN_CREATE | IN_DELETE);
    else
	fwData[fwDataSize].watchDesc = 0;

    fwData[fwDataSize].callback  = callback;
    fwData[fwDataSize].closure   = closure;
    /* determine current highest ID */
    for (i = 0; i < fwDataSize; i++)
	if (fwData[i].watchId > maxWatchId)
	    maxWatchId = fwData[i].watchId;

    fwData[fwDataSize].watchId = maxWatchId + 1;

    fwDataSize++;

    return (maxWatchId + 1);
}

void bsRemoveFileWatch (unsigned int watchId)
{
    int selectedIndex, i;

    selectedIndex = findDataIndexById (watchId);
    /* not found */
    if (selectedIndex < 0)
	return;

    /* clear entry */
    free (fwData[selectedIndex].fileName);
    if (fwData[selectedIndex].watchDesc)
    	inotify_rm_watch (inotifyFd, fwData[selectedIndex].watchDesc);

    /* shrink array */
    for (i = selectedIndex; i < (fwDataSize - 1); i++)
	fwData[i] = fwData[i+1];

    fwDataSize--;

    if (fwDataSize > 0)
    	fwData = realloc (fwData, fwDataSize * sizeof(FilewatchData));
    else
	free (fwData);
    
    if (!fwDataSize)
    {
	close (inotifyFd);
	inotifyFd = 0;
    }
}

void bsDisableFileWatch (unsigned int watchId)
{
    int index;

    index = findDataIndexById (watchId);

    if (index < 0)
	return;

    if (fwData[index].watchDesc)
    {
    	inotify_rm_watch (inotifyFd, fwData[index].watchDesc);
	fwData[index].watchDesc = 0;
    }
}

void bsEnableFileWatch (unsigned int watchId)
{
    int index;

    index = findDataIndexById (watchId);

    if (index < 0)
	return;

    if (!fwData[index].watchDesc)
	fwData[index].watchDesc = inotify_add_watch (inotifyFd, 
						     fwData[index].fileName,
	   					     IN_MODIFY | IN_MOVE |
   						     IN_CREATE | IN_DELETE);
}

