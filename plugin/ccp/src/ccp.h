/*
 * Compiz configuration system library plugin
 *
 * Copyright (C) 2007  Dennis Kasprzyk <onestone@opencompositing.org>
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

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

extern "C" {
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <ccs.h>
}

#include <core/core.h>
#include <core/pluginclasshandler.h>
#include <core/timer.h>

class CcpScreen :
    public ScreenInterface,
    public PluginClassHandler<CcpScreen,CompScreen>
{
    public:
	CcpScreen (CompScreen *screen);
	~CcpScreen ();

	bool initPluginForScreen (CompPlugin *p);

	bool setOptionForPlugin (const char *plugin,
				 const char *name,
				 CompOption::Value &v);

	bool timeout ();
	bool reload ();

	void setOptionFromContext (CompOption *o, const char *plugin);
	void setContextFromOption (CompOption *o, const char *plugin);


    public:
	CCSContext  *mContext;
	bool        mApplyingSettings;

	CompTimer mTimeoutTimer;
	CompTimer mReloadTimer;
};

class CcpPluginVTable :
    public CompPlugin::VTableForScreen<CcpScreen>
{
    public:

	bool init ();
};
