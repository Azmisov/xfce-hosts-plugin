#ifndef __HOSTS_H__
#define __HOSTS_H__

G_BEGIN_DECLS

/* plugin structure */
typedef struct {
	XfcePanelPlugin *plugin;

	/* panel widgets */
	GtkWidget       *ebox;
	GtkWidget       *hvbox;
	GtkWidget       *label;

	/* sample settings */
	gchar           *setting1;
	gint             setting2;
	gboolean         setting3;
} HostsPlugin;

void sample_save (XfcePanelPlugin *plugin, HostsPlugin *sample);

G_END_DECLS

#endif