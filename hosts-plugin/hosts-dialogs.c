#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>

#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4panel/libxfce4panel.h>

#include "hosts.h"
#include "hosts-dialogs.h"

#define PLUGIN_WEBSITE "https://github.com/Azmisov/xfce-hosts-plugin"

typedef struct {
	HostsPlugin *hosts;
	// widget to hold list of hostnames
	GtkWidget *listbox;
	// widget to add another hostname
	GtkWidget *entry;
} HostsDialogData;

static gboolean is_valid_hostname(const gchar *hostname) {
	if (hostname == NULL || *hostname == '\0') {
		return FALSE;
	}

	// Check length
	size_t len = strlen(hostname);
	if (len > 255) {
		return FALSE;
	}

	// Check each label
	const gchar *label = hostname;
	while (*label) {
		const gchar *dot = strchr(label, '.');
		size_t label_len = dot ? (size_t)(dot - label) : strlen(label);

		// Label length must be between 1 and 63 characters
		if (label_len < 1 || label_len > 63) {
			return FALSE;
		}

		// Label must start and end with a letter or digit
		if (!g_ascii_isalnum(label[0]) || !g_ascii_isalnum(label[label_len - 1])) {
			return FALSE;
		}

		// Label must contain only letters, digits, or hyphens
		for (size_t i = 1; i < label_len - 1; i++) {
			if (!g_ascii_isalnum(label[i]) && label[i] != '-') {
				return FALSE;
			}
		}

		// Move to the next label
		if (dot) {
			label = dot + 1;
		} else {
			break;
		}
	}

	return TRUE;
}

static void hosts_configure_response(GtkWidget *dialog, gint response, HostsPlugin *hosts) {
	gboolean result;

	// show help
	if (response == GTK_RESPONSE_HELP){
		result = g_spawn_command_line_async("exo-open --launch WebBrowser " PLUGIN_WEBSITE, NULL);

		if (G_UNLIKELY(result == FALSE))
			g_warning (_("Unable to open the following url: %s"), PLUGIN_WEBSITE);
	}
	else {
		// remove the dialog data from the plugin
		g_object_set_data(G_OBJECT(hosts->plugin), "dialog", NULL);

		// unlock the panel menu
		xfce_panel_plugin_unblock_menu(hosts->plugin);

		// save the plugin
		hosts_save(hosts->plugin, hosts);

		// destroy the properties dialog
		gtk_widget_destroy (dialog);
	}
}

// Add new alias to the listbox widget. Returns the newly added row. Set index to -1 to append
static GtkWidget* hosts_add_listbox_item(GtkWidget *listbox, gchar *text, gint index) {
	GtkWidget *row = gtk_list_box_row_new();
	GtkWidget *label = gtk_label_new(text);
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_container_add(GTK_CONTAINER(row), label);
	gtk_list_box_insert(GTK_LIST_BOX(listbox), row, index);
	gtk_widget_show_all(row);
	return row;
}

// Add another hostname alias to the list
static void hosts_add_alias(GtkButton *button, gpointer user_data) {
	HostsDialogData *data = (HostsDialogData *) user_data;
	const gchar *new_alias = gtk_entry_get_text(GTK_ENTRY(data->entry));

	// validate the new hostname
	if (!is_valid_hostname(new_alias)) {
		GtkWidget *message_dialog = gtk_message_dialog_new(
			NULL, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
			"Invalid hostname: %s", new_alias
		);
		gtk_dialog_run(GTK_DIALOG(message_dialog));
		gtk_widget_destroy(message_dialog);
		return;
	}

	// validate hostname is unique
	if (data->hosts->names != NULL) {
		for (gint i=0; data->hosts->names[i]; i++) {
			if (g_strcmp0(data->hosts->names[i], new_alias) == 0) {
				GtkWidget *message_dialog = gtk_message_dialog_new(
					NULL, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
					"Hostname already added: %s", new_alias
				);
				gtk_dialog_run(GTK_DIALOG(message_dialog));
				gtk_widget_destroy(message_dialog);
				return;
			}
		}
	}

	// Get the current length of the names array
	gint length = 0;
	while (data->hosts->names && data->hosts->names[length]) {
		length++;
	}

	// Reallocate memory for the new alias
	data->hosts->names = g_realloc(data->hosts->names, (length + 2) * sizeof(gchar *));
	data->hosts->names[length] = g_strdup(new_alias);
	data->hosts->names[length + 1] = NULL;

	// Reallocate memory for the enabled array; length is given by names array
	data->hosts->enabled = g_realloc(data->hosts->enabled, (length + 1) * sizeof(gboolean));
	data->hosts->enabled[length] = FALSE;

	// Add to listbox widget
	hosts_add_listbox_item(data->listbox, data->hosts->names[length], -1);

	// Clear the entry
	gtk_entry_set_text(GTK_ENTRY(data->entry), "");
}

// Shift an alias in the list up or down some number of positions
static void hosts_shift_alias_generic(HostsDialogData *data, gint shift){
	// get selected row
	GtkListBoxRow *selected_row = gtk_list_box_get_selected_row(GTK_LIST_BOX(data->listbox));
	if (!selected_row) {
		return;
	}
	gint cur_index = gtk_list_box_row_get_index(selected_row);
	gint new_index = cur_index + shift;
	gint cur_size = (gint) g_list_length(gtk_container_get_children(GTK_CONTAINER(data->listbox)));
	if (new_index < 0 || new_index >= cur_size) {
		return;
	}

	// Swap the rows
	gchar *temp_name = data->hosts->names[new_index];
	data->hosts->names[new_index] = data->hosts->names[cur_index];
	data->hosts->names[cur_index] = temp_name;
	gboolean temp_enabled = data->hosts->enabled[new_index];
	data->hosts->enabled[new_index] = data->hosts->enabled[cur_index];
	data->hosts->enabled[cur_index] = temp_enabled;

	// Remove the list item and then reinsert at the new position
	gtk_container_remove(GTK_CONTAINER(data->listbox), GTK_WIDGET(selected_row));
	GtkWidget *row = hosts_add_listbox_item(data->listbox, data->hosts->names[new_index], new_index);
	gtk_list_box_select_row(GTK_LIST_BOX(data->listbox), GTK_LIST_BOX_ROW(row));
}

/** Shift a selected alias up in the list */
static void hosts_shift_up(GtkButton *button, gpointer user_data) {
	hosts_shift_alias_generic((HostsDialogData *) user_data, -1);
}

/** Shift a selected alias down in the list */
static void hosts_shift_down(GtkButton *button, gpointer user_data) {
	hosts_shift_alias_generic((HostsDialogData *) user_data, 1);
}

// Configuration dialog
void hosts_configure (XfcePanelPlugin *plugin, HostsPlugin *hosts){
	HostsDialogData *data = g_new0(HostsDialogData, 1);
	data->hosts = hosts;

	GtkWidget *dialog;

	// block the plugin menu
	xfce_panel_plugin_block_menu(plugin);

	// create the dialog
	GtkWidget *vbox, *hbox, *scroll, *button_box, *button_up, *button_down, *button_delete, *button_add;

	// Create the main vertical box
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

	// Create the horizontal box for the list and buttons
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
	gtk_widget_set_vexpand(hbox, TRUE);

	// Create the scrollable list box
	scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand(scroll, TRUE);
	data->listbox = gtk_list_box_new();
	gtk_container_add(GTK_CONTAINER(scroll), data->listbox);
	gtk_box_pack_start(GTK_BOX(hbox), scroll, TRUE, TRUE, 0);

	// Initialize listbox with all elements in hosts->names
	if (hosts->names != NULL) {
		for (gint i = 0; hosts->names[i]; i++)
			hosts_add_listbox_item(data->listbox, hosts->names[i], -1);
	}

	// Create the button box
	button_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);

	GtkIconTheme *theme = gtk_icon_theme_get_default();

	button_up = gtk_button_new();
	if (gtk_icon_theme_has_icon(theme, "go-up-symbolic")) {
		gtk_button_set_image(GTK_BUTTON(button_up), gtk_image_new_from_icon_name("go-up-symbolic", GTK_ICON_SIZE_BUTTON));
	} else {
		gtk_button_set_label(GTK_BUTTON(button_up), "Move Up");
	}
	gtk_widget_set_tooltip_text(button_up, "Move selected item up");
	g_signal_connect(button_up, "clicked", G_CALLBACK(hosts_shift_up), data);

	button_down = gtk_button_new();
	if (gtk_icon_theme_has_icon(theme, "go-down-symbolic")) {
		gtk_button_set_image(GTK_BUTTON(button_down), gtk_image_new_from_icon_name("go-down-symbolic", GTK_ICON_SIZE_BUTTON));
	} else {
		gtk_button_set_label(GTK_BUTTON(button_down), "Move Down");
	}
	gtk_widget_set_tooltip_text(button_down, "Move selected item down");
	g_signal_connect(button_down, "clicked", G_CALLBACK(hosts_shift_down), data);

	button_delete = gtk_button_new();
	if (gtk_icon_theme_has_icon(theme, "user-trash-symbolic")) {
		gtk_button_set_image(GTK_BUTTON(button_delete), gtk_image_new_from_icon_name("user-trash-symbolic", GTK_ICON_SIZE_BUTTON));
	} else {
		gtk_button_set_label(GTK_BUTTON(button_delete), "Delete");
	}
	gtk_widget_set_tooltip_text(button_delete, "Delete selected item");

	gtk_box_pack_start(GTK_BOX(button_box), button_up, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(button_box), button_down, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(button_box), button_delete, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), button_box, FALSE, FALSE, 0);

	// Pack the horizontal box into the main vertical box
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

	// Create the entry and add button
	GtkWidget *entry_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
	data->entry = gtk_entry_new();
	gtk_entry_set_placeholder_text(GTK_ENTRY(data->entry), "New 127.0.0.1 hostname alias");
	button_add = gtk_button_new_with_label("Add");
	gtk_box_pack_start(GTK_BOX(entry_hbox), data->entry, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(entry_hbox), button_add, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), entry_hbox, FALSE, FALSE, 0);

	// Set focus to the entry widget when the dialog opens
	// NOTE: disabling this since it causes placeholder text to get hidden; want that to show for user help
	// gtk_widget_grab_focus(data->entry);

	// Add the new host when button is clicked or they press enter
	g_signal_connect(button_add, "clicked", G_CALLBACK(hosts_add_alias), data);
	g_signal_connect(data->entry, "activate", G_CALLBACK(hosts_add_alias), data);

	// Show all the widgets in the vbox
	gtk_widget_show_all(vbox);

	// Create the dialog
	dialog = gtk_dialog_new_with_buttons(
		"Configure Hosts Plugin",
		GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(plugin))),
		GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		"_Close", GTK_RESPONSE_CLOSE,
		NULL
	);

	GtkWidget *help_button = gtk_dialog_add_button(GTK_DIALOG(dialog), "_Help", GTK_RESPONSE_HELP);
	gtk_button_set_image(GTK_BUTTON(help_button), gtk_image_new_from_icon_name("help-browser", GTK_ICON_SIZE_BUTTON));

	// Add the vbox to the dialog's content area
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), vbox);

	// center dialog on the screen
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);

	// set dialog size
	gtk_window_set_default_size(GTK_WINDOW(dialog), 500, 500);

	// set dialog icon
	gtk_window_set_icon_name(GTK_WINDOW(dialog), "applications-internet");

	// link the dialog to the plugin, so we can destroy it when the plugin
	// is closed, but the dialog is still open
	g_object_set_data(G_OBJECT(plugin), "dialog", dialog);

	// connect the response signal to the dialog
	g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(hosts_configure_response), hosts);

	// show the entire dialog
	gtk_widget_show(dialog);
}

void hosts_about (XfcePanelPlugin *plugin) {
	// about dialog code. you can use the GtkAboutDialog or the XfceAboutInfo widget
	const gchar *auth[] = {
		"Isaac Nygaard",
		NULL
	};

	gtk_show_about_dialog (
		NULL,
		"logo-icon-name", "applications-internet",
		"license",        xfce_get_license_text(XFCE_LICENSE_TEXT_GPL),
		"version",        PACKAGE_VERSION,
		"program-name",   PACKAGE_NAME,
		"comments",       _("Toggle host aliases for local web development"),
		"website",        PLUGIN_WEBSITE,
		"copyright",      "Copyright \xc2\xa9 2025 — Isaac Nygaard",
		"authors",        auth,
		NULL
	);
}
