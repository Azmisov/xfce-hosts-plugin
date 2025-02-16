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

void hosts_save(XfcePanelPlugin *plugin, HostsPlugin *hosts){
	XfceRc *rc;
	gchar  *file;

	// get the config file location
	file = xfce_panel_plugin_save_location(plugin, TRUE);
	if (G_UNLIKELY(file == NULL)){
		DBG ("Failed to open config file for saving");
		return;
 	}

	// open the config file, read/write
	rc = xfce_rc_simple_open(file, FALSE);
	g_free (file);
	if (G_LIKELY(rc != NULL)){
		DBG("Saving settings");
		if (hosts->names) {
			xfce_rc_write_list_entry(rc, "names", hosts->names, NULL);
			for (guint i = 0; hosts->names[i]; i++) {
				xfce_rc_write_bool_entry(rc, hosts->names[i], hosts->enabled[i]);
			}
		}
		xfce_rc_close(rc);
	}
}

static void hosts_read(HostsPlugin *hosts) {
	// get the plugin config file location
	gchar *file = xfce_panel_plugin_save_location(hosts->plugin, TRUE);
	if (G_LIKELY (file != NULL)) {
		// open the config file, readonly
		XfceRc *rc = xfce_rc_simple_open(file, TRUE);
   		g_free(file);
   		if (G_LIKELY (rc != NULL)) {
			// read the settings
			hosts->names = xfce_rc_read_list_entry(rc, "names", NULL);
			// not null? read booleans for each entry
			if (hosts->names) {
				hosts->enabled = g_new0(gboolean, g_strv_length(hosts->names));
				for (guint i = 0; hosts->names[i]; i++) {
					hosts->enabled[i] = xfce_rc_read_bool_entry(rc, hosts->names[i], FALSE);
					DBG("Host %s is %s", hosts->names[i], hosts->enabled[i] ? "enabled" : "disabled");
				}
			}
			xfce_rc_close (rc);
			return;
	 	}
 	}

	// fallback when no settings found
	DBG("Failed to load settings; assuming no hosts configured");
	hosts->names = NULL;
	hosts->enabled = NULL;
}

static gboolean execute_sudo_command(const char *command, GError **error) {
    // 1. Construct the sudo command
    // Important: Use a safe way to build the command to avoid shell injection!
    // Never just concatenate strings directly.
    char *sudo_command = g_strdup_printf("pkexec %s", command);  // Safer than direct concatenation

    // 2. Execute the command using g_spawn_command_line_sync
    // This function handles the password prompt automatically if sudo requires it.
    gchar *standard_output = NULL;
    gchar *standard_error = NULL;
    gint exit_status;

    gboolean success = g_spawn_command_line_sync(
        sudo_command,
        &standard_output,
        &standard_error,
        &exit_status,
        error
	);

    g_free(sudo_command); // Free allocated memory

    if (success) {
        if (exit_status != 0) {
            g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                        "Command failed with exit status %d. Error: %s",
                        exit_status, standard_error ? standard_error : "Unknown");
            success = FALSE;
        } else {
          // If needed, you can process standard_output here
          g_print("Command output: %s\n", standard_output ? standard_output : "");
        }
    } else {
      // g_spawn_command_line_sync already sets the error if it fails.
      // You might want to add additional logging here.
      g_warning("g_spawn_command_line_sync failed");
    }

    g_free(standard_output);
    g_free(standard_error);

    return success;
}

// Sync the /etc/hosts file with the current configured hosts and which are enabled/disabled.
// This syncs the entire file, as there could be modifications made outside of the plugin that
// override this plugin's changes. Returns false and shows a dialog message if unsuccessful.
gboolean etc_hosts_sync(HostsPlugin *hosts) {
	// nothing to sync?
	if (!hosts->names)
		return TRUE;

	DBG("Syncing /etc/hosts");

	// Read file; split line-by-line; should have read permissions to /etc/hosts
	gsize length;
	GError *error = NULL;
	gchar *contents = NULL;
	if (!g_file_get_contents("/etc/hosts", &contents, &length, &error)) {
		g_warning("Failed to read /etc/hosts: %s", error->message);
		g_error_free(error);
		return FALSE;
	}
	gchar **lines = g_strsplit(contents, "\n", -1);
	guint lines_length = g_strv_length(lines);
	g_free(contents);

	// Rebuild the file with modified lines
	gboolean localhost_seen = FALSE;
	gboolean modified = FALSE;
	gchar **new_lines = g_new0(gchar *, lines_length + 2); // one extra line in case we need to add another
	for (guint i = 0; lines[i]; i++) {
		if (g_str_has_prefix(lines[i], "127.0.0.1")) {
			GHashTable *hosts_set = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
			gchar **tokens = g_strsplit_set(lines[i], " \t", -1);

			// Add all hosts from the line to the set
			for (guint k = 1; tokens[k] != NULL; k++) {
				g_hash_table_add(hosts_set, g_strdup(tokens[k]));
			}

			// Add enabled hosts
			for (guint k = 0; hosts->names[k] != NULL; k++) {
				// only add if its the first time we see localhost
				if (!localhost_seen && hosts->enabled[k])
					modified |= g_hash_table_add(hosts_set, g_strdup(hosts->names[k]));
				else
					modified |= g_hash_table_remove(hosts_set, hosts->names[k]);
			}

			// Create the new line
			GString *new_line = g_string_new(tokens[0]);
			GHashTableIter iter;
			gpointer key;
			g_hash_table_iter_init(&iter, hosts_set);
			while (g_hash_table_iter_next(&iter, &key, NULL)) {
				g_string_append_printf(new_line, " %s", (gchar *)key);
			}

			// Add the new line to the new_lines array
			new_lines[i] = g_string_free(new_line, FALSE);

			// Cleanup
			g_strfreev(tokens);
			g_hash_table_destroy(hosts_set);

			localhost_seen = TRUE;
		} else {
			new_lines[i] = g_strdup(lines[i]);
		}
	}
	g_strfreev(lines);

	// no localhost line found; add one
	if (!localhost_seen) {
		modified = TRUE;
		GString *new_line = g_string_new("127.0.0.1");
		for (guint k = 0; hosts->names[k] != NULL; k++) {
			if (hosts->enabled[k]) {
				g_string_append_printf(new_line, " %s", hosts->names[k]);
			}
		}
		new_lines[lines_length] = g_string_free(new_line, FALSE);
		new_lines[lines_length+1] = NULL;
	}
	else new_lines[lines_length] = NULL;

	// Don't write the file (which will prompt for sudo access) if no modifications were made
	if (!modified) {
		DBG("No modifications to /etc/hosts needed");
		g_strfreev(new_lines);
		return TRUE;
	}

	g_print("New /etc/hosts file:\n");
	for (guint i = 0; new_lines[i]; i++)
		g_print("> %s\n", new_lines[i]);

	// write new contents to file
	gchar *new_contents = g_strjoinv("\n", new_lines);
	g_strfreev(new_lines);

	gboolean success = FALSE;
	// write tmp file, then copy to /etc/hosts using sudo
	if (g_file_set_contents("/tmp/xfce-hosts-plugin_etc_hosts", new_contents, -1, &error))
		success = execute_sudo_command("cp /tmp/xfce-hosts-plugin_etc_hosts /etc/hosts", &error);
	g_free(new_contents);

	if (!success) {
		// Open a dialog with the error message
		GtkWidget *dialog = gtk_message_dialog_new(
			NULL,
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE,
			"xfce-hosts-plugin couldn't sync with /etc/hosts: %s", error->message
		);
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
		g_error_free(error);
	}

	return success;
}

// Callback for toggling a host
static void hosts_toggle(GtkCheckMenuItem *menu_item, HostToggleData *data) {
    gboolean active = gtk_check_menu_item_get_active(menu_item);
	// don't do anything if state matches
	if (data->hosts->enabled[data->index] == active)
		return;
	data->hosts->enabled[data->index] = active;
	// revert if /etc/hosts sync fails
	if (!etc_hosts_sync(data->hosts)) {
		data->hosts->enabled[data->index] = !active;
		gtk_check_menu_item_set_active(menu_item, !active);
	}
}

// Show dropdown with hosts that can be toggled
static void hosts_dropdown(GtkWidget *widget, gpointer data) {
	HostsPlugin *hosts =  (HostsPlugin*) data;
	GtkWidget *menu;

	menu = gtk_menu_new();

	// dynamic list element for each configured host
	if (hosts->names) {
		for (guint i = 0; hosts->names[i]; i++) {
			GtkWidget *menu_item = gtk_check_menu_item_new_with_label(hosts->names[i]);
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item), hosts->enabled[i]);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
			HostToggleData *toggle_data = g_new(HostToggleData, 1);
			toggle_data->hosts = hosts;
			toggle_data->index = i;
			g_signal_connect(menu_item, "toggled", G_CALLBACK(hosts_toggle), toggle_data);
			g_object_set_data_full(G_OBJECT(menu_item), "toggle_data", toggle_data, g_free);
		}
	}

	// Add a separator
	GtkWidget *separator = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator);
	gtk_widget_show(separator);

	// Add the 'Configure...' menu item
	GtkWidget *configure_item = gtk_menu_item_new_with_label("Configure...");
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), configure_item);
	g_signal_connect(configure_item, "activate", G_CALLBACK(hosts_configure), hosts);
	gtk_widget_show(configure_item);

	gtk_widget_show_all(menu);
	gtk_menu_popup_at_widget(GTK_MENU(menu), hosts->button, GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL);
}

/** Initialize the GTK widget for the plugin */
static HostsPlugin *hosts_new (XfcePanelPlugin *plugin){
	HostsPlugin   *hosts;
	GtkOrientation  orientation;

	/* allocate memory for the plugin structure */
	hosts = g_slice_new0(HostsPlugin);

	// pointer to plugin
	hosts->plugin = plugin;

	// Read the user settings
	hosts_read(hosts);

	// Sync, in case file was modified while not running
	etc_hosts_sync(hosts);

	// Get the current orientation
	orientation = xfce_panel_plugin_get_orientation (plugin);

	// Panel box
	hosts->ebox = gtk_event_box_new();
	gtk_widget_show(hosts->ebox);
	hosts->hvbox = gtk_box_new(orientation, 2);
	gtk_widget_show(hosts->hvbox);
	gtk_container_add(GTK_CONTAINER(hosts->ebox), hosts->hvbox);

	// Icon button, clicking pulls up dropdown menu for toggling hosts
	GtkIconTheme *theme = gtk_icon_theme_get_default();
	if (gtk_icon_theme_has_icon(theme, "applications-internet"))
		hosts->icon = gtk_image_new_from_icon_name("applications-internet", GTK_ICON_SIZE_BUTTON);
	else {
		g_warning("Failed to load button icon");
		hosts->icon = gtk_label_new("Hosts");
	}
    hosts->button = gtk_button_new();
    gtk_button_set_image(GTK_BUTTON(hosts->button), hosts->icon);
    gtk_widget_set_tooltip_text(hosts->button, "Toggle hosts");
    g_signal_connect(hosts->button, "clicked", G_CALLBACK(hosts_dropdown), hosts);

    // Add button to the box
    gtk_box_pack_start(GTK_BOX(hosts->hvbox), hosts->button, FALSE, FALSE, 0);
	gtk_widget_show(hosts->button);
	gtk_widget_show(hosts->icon);

	return hosts;
}

static void hosts_free(XfcePanelPlugin *plugin, HostsPlugin *hosts) {
	GtkWidget *dialog;

	// check if the dialog is still open. if so, destroy it
	dialog = g_object_get_data (G_OBJECT(plugin), "dialog");
	if (G_UNLIKELY(dialog != NULL))
		gtk_widget_destroy (dialog);

	// destroy the panel widgets
	gtk_widget_destroy(hosts->hvbox);

	// cleanup hosts configuration
	if (G_LIKELY(hosts->names != NULL)){
		g_strfreev(hosts->names);
		g_free(hosts->enabled);
	}

	// free the plugin structure
	g_slice_free(HostsPlugin, hosts);
}

static void hosts_orientation_changed (
	XfcePanelPlugin *plugin, GtkOrientation orientation, HostsPlugin *hosts
){
  /* change the orientation of the box */
  gtk_orientable_set_orientation(GTK_ORIENTABLE(hosts->hvbox), orientation);
}

static gboolean hosts_size_changed (
	XfcePanelPlugin *plugin, gint size, HostsPlugin *hosts
){
	GtkOrientation orientation;
	/* get the orientation of the plugin */
	orientation = xfce_panel_plugin_get_orientation (plugin);
	/* set the widget size */
	if (orientation == GTK_ORIENTATION_HORIZONTAL)
		gtk_widget_set_size_request(GTK_WIDGET (plugin), -1, size);
	else
		gtk_widget_set_size_request(GTK_WIDGET (plugin), size, -1);
	/* we handled the orientation */
	return TRUE;
}

// Adds the plugin widget to the XFCE panel; registers XFCE signals
static void hosts_construct (XfcePanelPlugin *plugin) {
	HostsPlugin *hosts;

	// setup transation domain
	xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

	// create the plugin
	hosts = hosts_new(plugin);

	// add the ebox to the panel
	gtk_container_add(GTK_CONTAINER(plugin), hosts->ebox);

	// show the panel's right-click menu on this ebox
	xfce_panel_plugin_add_action_widget(plugin, hosts->ebox);

	// connect plugin signals
	g_signal_connect(G_OBJECT(plugin), "free-data", G_CALLBACK(hosts_free), hosts);
	g_signal_connect(G_OBJECT(plugin), "save", G_CALLBACK(hosts_save), hosts);
	g_signal_connect(G_OBJECT(plugin), "size-changed", G_CALLBACK(hosts_size_changed), hosts);
	g_signal_connect(G_OBJECT(plugin), "orientation-changed", G_CALLBACK(hosts_orientation_changed), hosts);

	/* show the configure menu item and connect signal */
	xfce_panel_plugin_menu_show_configure(plugin);
	g_signal_connect(G_OBJECT(plugin), "configure-plugin", G_CALLBACK(hosts_configure), hosts);

	/* show the about menu item and connect signal */
	xfce_panel_plugin_menu_show_about(plugin);
	g_signal_connect(G_OBJECT(plugin), "about", G_CALLBACK(hosts_about), NULL);
}
