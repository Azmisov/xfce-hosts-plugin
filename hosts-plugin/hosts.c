#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4panel/libxfce4panel.h>

#include "hosts.h"
#include "hosts-dialogs.h"

/* default settings */
#define DEFAULT_SETTING1 NULL
#define DEFAULT_SETTING2 1
#define DEFAULT_SETTING3 FALSE

/* prototypes */
static void hosts_construct (XfcePanelPlugin *plugin);

/* register the plugin */
XFCE_PANEL_PLUGIN_REGISTER (hosts_construct);

void hosts_save (XfcePanelPlugin *plugin, HostsPlugin    *sample){
	XfceRc *rc;
	gchar  *file;
	/* get the config file location */
	file = xfce_panel_plugin_save_location (plugin, TRUE);
	if (G_UNLIKELY (file == NULL)){
		DBG ("Failed to open config file");
		return;
 	}
	/* open the config file, read/write */
	rc = xfce_rc_simple_open (file, FALSE);
	g_free (file);
	if (G_LIKELY (rc != NULL)) {
		/* save the settings */
		DBG(".");
		if (sample->setting1)
			xfce_rc_write_entry    (rc, "setting1", sample->setting1);
		xfce_rc_write_int_entry  (rc, "setting2", sample->setting2);
		xfce_rc_write_bool_entry (rc, "setting3", sample->setting3);
		/* close the rc file */
		xfce_rc_close (rc);
	}
}

static void hosts_read (HostsPlugin *sample) {
	XfceRc      *rc;
	gchar       *file;
	const gchar *value;
	/* get the plugin config file location */
	file = xfce_panel_plugin_save_location (sample->plugin, TRUE);

	if (G_LIKELY (file != NULL)) {
		/* open the config file, readonly */
   		rc = xfce_rc_simple_open (file, TRUE);
   		/* cleanup */
   		g_free (file);
   		if (G_LIKELY (rc != NULL)) {
			/* read the settings */
			value = xfce_rc_read_entry (rc, "setting1", DEFAULT_SETTING1);
			sample->setting1 = g_strdup (value);
			sample->setting2 = xfce_rc_read_int_entry (rc, "setting2", DEFAULT_SETTING2);
			sample->setting3 = xfce_rc_read_bool_entry (rc, "setting3", DEFAULT_SETTING3);
			/* cleanup */
			xfce_rc_close (rc);
			/* leave the function, everything went well */
			return;
	 	}
 	}
	/* something went wrong, apply default values */
	DBG ("Applying default settings");
	sample->setting1 = g_strdup (DEFAULT_SETTING1);
	sample->setting2 = DEFAULT_SETTING2;
	sample->setting3 = DEFAULT_SETTING3;
}

static HostsPlugin *hosts_new (XfcePanelPlugin *plugin){
	HostsPlugin   *sample;
	GtkOrientation  orientation;
	GtkWidget      *label;
	/* allocate memory for the plugin structure */
	sample = g_slice_new0 (HostsPlugin);
	/* pointer to plugin */
	sample->plugin = plugin;
	/* read the user settings */
	hosts_read (sample);
	/* get the current orientation */
	orientation = xfce_panel_plugin_get_orientation (plugin);
	/* create some panel widgets */
	sample->ebox = gtk_event_box_new ();
	gtk_widget_show (sample->ebox);
	sample->hvbox = gtk_box_new (orientation, 2);
	gtk_widget_show (sample->hvbox);
	gtk_container_add (GTK_CONTAINER (sample->ebox), sample->hvbox);
	/* some sample widgets */
	label = gtk_label_new (_("Sample"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (sample->hvbox), label, FALSE, FALSE, 0);
	label = gtk_label_new (_("Plugin"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (sample->hvbox), label, FALSE, FALSE, 0);
	return sample;
}

static void hosts_free (XfcePanelPlugin *plugin, HostsPlugin *sample) {
	GtkWidget *dialog;
	/* check if the dialog is still open. if so, destroy it */
	dialog = g_object_get_data (G_OBJECT (plugin), "dialog");
	if (G_UNLIKELY (dialog != NULL))
		gtk_widget_destroy (dialog);
	/* destroy the panel widgets */
	gtk_widget_destroy (sample->hvbox);
	/* cleanup the settings */
	if (G_LIKELY (sample->setting1 != NULL))
		g_free (sample->setting1);
	/* free the plugin structure */
	g_slice_free (HostsPlugin, sample);
}

static void hosts_orientation_changed (
	XfcePanelPlugin *plugin, GtkOrientation orientation, HostsPlugin *sample
){
  /* change the orientation of the box */
  gtk_orientable_set_orientation(GTK_ORIENTABLE(sample->hvbox), orientation);
}

static gboolean hosts_size_changed (
	XfcePanelPlugin *plugin, gint size, HostsPlugin *sample
){
	GtkOrientation orientation;
	/* get the orientation of the plugin */
	orientation = xfce_panel_plugin_get_orientation (plugin);
	/* set the widget size */
	if (orientation == GTK_ORIENTATION_HORIZONTAL)
		gtk_widget_set_size_request (GTK_WIDGET (plugin), -1, size);
	else
		gtk_widget_set_size_request (GTK_WIDGET (plugin), size, -1);
	/* we handled the orientation */
	return TRUE;
}

static void hosts_construct (XfcePanelPlugin *plugin) {
	DBG("Initializing new plugin structure");
	HostsPlugin *hosts;

	/* setup transation domain */
	xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

	/* create the plugin */
	hosts = hosts_new(plugin);

	/* add the ebox to the panel */
	gtk_container_add(GTK_CONTAINER(plugin), hosts->ebox);

	/* show the panel's right-click menu on this ebox */
	xfce_panel_plugin_add_action_widget(plugin, hosts->ebox);

	/* connect plugin signals */
	g_signal_connect(G_OBJECT(plugin), "free-data", G_CALLBACK (hosts_free), hosts);
	g_signal_connect(G_OBJECT(plugin), "save", G_CALLBACK (hosts_save), hosts);
	g_signal_connect(G_OBJECT(plugin), "size-changed", G_CALLBACK (hosts_size_changed), hosts);
	g_signal_connect(G_OBJECT(plugin), "orientation-changed", G_CALLBACK (hosts_orientation_changed), hosts);

	/* show the configure menu item and connect signal */
	xfce_panel_plugin_menu_show_configure (plugin);
	g_signal_connect (G_OBJECT (plugin), "configure-plugin", G_CALLBACK (hosts_configure), hosts);

	/* show the about menu item and connect signal */
	xfce_panel_plugin_menu_show_about (plugin);
	g_signal_connect (G_OBJECT (plugin), "about", G_CALLBACK (hosts_about), NULL);
}
