/*
 * search-plugin: Abstract class for search plugins
 * 
 * Copyright 2012-2013 Stephan Haller <nomad@froevel.de>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
 */

#ifndef __XFDASHBOARD_SEARCH_PLUGIN__
#define __XFDASHBOARD_SEARCH_PLUGIN__

#include <glib-object.h>

G_BEGIN_DECLS

#define XFDASHBOARD_TYPE_SEARCH_PLUGIN				(xfdashboard_search_plugin_get_type())
#define XFDASHBOARD_SEARCH_PLUGIN(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), XFDASHBOARD_TYPE_SEARCH_PLUGIN, XfdashboardSearchPlugin))
#define XFDASHBOARD_IS_SEARCH_PLUGIN(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), XFDASHBOARD_TYPE_SEARCH_PLUGIN))
#define XFDASHBOARD_SEARCH_PLUGIN_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), XFDASHBOARD_TYPE_SEARCH_PLUGIN, XfdashboardSearchPluginClass))
#define XFDASHBOARD_IS_SEARCH_PLUGIN_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), XFDASHBOARD_TYPE_SEARCH_PLUGIN))
#define XFDASHBOARD_SEARCH_PLUGIN_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), XFDASHBOARD_TYPE_SEARCH_PLUGIN, XfdashboardSearchPluginClass))

typedef struct _XfdashboardSearchPlugin				XfdashboardSearchPlugin; 
typedef struct _XfdashboardSearchPluginPrivate		XfdashboardSearchPluginPrivate;
typedef struct _XfdashboardSearchPluginClass		XfdashboardSearchPluginClass;

struct _XfdashboardSearchPlugin
{
	/* Parent instance */
	GObject							parent_instance;

	/* Private structure */
	XfdashboardSearchPluginPrivate	*priv;
};

struct _XfdashboardSearchPluginClass
{
	/*< private >*/
	/* Parent class */
	GObjectClass					parent_class;

	/*< public >*/
	/* Virtual functions */
};

/* Public API */
GType xfdashboard_search_plugin_get_type(void) G_GNUC_CONST;

G_END_DECLS

#endif
