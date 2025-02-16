#ifndef __HOSTS_H__
#define __HOSTS_H__

G_BEGIN_DECLS

/* plugin structure */
typedef struct {
	XfcePanelPlugin *plugin;

	/* panel widgets */
	GtkWidget       *ebox;
	GtkWidget       *hvbox;
	GtkWidget       *icon;
	GtkWidget		*button;

	// hostnames
	gchar           **names;
	// which hosts are enabled
	gboolean         *enabled;

} HostsPlugin;

// Structure to indicate which host is being toggled
typedef struct {
	HostsPlugin *hosts;
	guint index;
} HostToggleData;

// Save configuration
void hosts_save(XfcePanelPlugin *plugin, HostsPlugin *hosts);

// Update the /etc/hosts file
gboolean etc_hosts_sync(HostsPlugin *hosts);

G_END_DECLS

#endif