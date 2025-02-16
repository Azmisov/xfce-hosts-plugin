#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>

#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4panel/libxfce4panel.h>

#include "hosts.h"
#include "hosts-dialogs.h"

/* the website url */
#define PLUGIN_WEBSITE "https://github.com/Azmisov/xfce-hosts-plugin"



static void
sample_configure_response (GtkWidget    *dialog,
                           gint          response,
                           SamplePlugin *sample)
{
  gboolean result;

  if (response == GTK_RESPONSE_HELP)
    {
      /* show help */
      result = g_spawn_command_line_async ("exo-open --launch WebBrowser " PLUGIN_WEBSITE, NULL);

      if (G_UNLIKELY (result == FALSE))
        g_warning (_("Unable to open the following url: %s"), PLUGIN_WEBSITE);
    }
  else
    {
      /* remove the dialog data from the plugin */
      g_object_set_data (G_OBJECT (sample->plugin), "dialog", NULL);

      /* unlock the panel menu */
      xfce_panel_plugin_unblock_menu (sample->plugin);

      /* save the plugin */
      sample_save (sample->plugin, sample);

      /* destroy the properties dialog */
      gtk_widget_destroy (dialog);
    }
}



void
sample_configure (XfcePanelPlugin *plugin,
                  SamplePlugin    *sample)
{
  GtkWidget *dialog;

  /* block the plugin menu */
  xfce_panel_plugin_block_menu (plugin);

  /* create the dialog */
  dialog = xfce_titled_dialog_new_with_mixed_buttons (_("Hosts Plugin"),
                                                      GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (plugin))),
                                                      GTK_DIALOG_DESTROY_WITH_PARENT,
                                                      "help-browser-symbolic", _("_Help"), GTK_RESPONSE_HELP,
                                                      "window-close-symbolic", _("_Close"), GTK_RESPONSE_OK,
                                                      NULL);

  /* center dialog on the screen */
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);

  /* set dialog icon */
  gtk_window_set_icon_name (GTK_WINDOW (dialog), "xfce4-settings");

  /* link the dialog to the plugin, so we can destroy it when the plugin
   * is closed, but the dialog is still open */
  g_object_set_data (G_OBJECT (plugin), "dialog", dialog);

  /* connect the response signal to the dialog */
  g_signal_connect (G_OBJECT (dialog), "response",
                    G_CALLBACK(sample_configure_response), sample);

  /* show the entire dialog */
  gtk_widget_show (dialog);
}



void
sample_about (XfcePanelPlugin *plugin)
{
  /* about dialog code. you can use the GtkAboutDialog
   * or the XfceAboutInfo widget */
  const gchar *auth[] =
    {
      "Xfce development team <xfce4-dev@xfce.org>",
      NULL
    };

  gtk_show_about_dialog (NULL,
                         "logo-icon-name", "xfce4-sample-plugin",
                         "license",        xfce_get_license_text (XFCE_LICENSE_TEXT_GPL),
                         "version",        PACKAGE_VERSION,
                         "program-name",   PACKAGE_NAME,
                         "comments",       _("This is a sample plugin"),
                         "website",        PLUGIN_WEBSITE,
                         "copyright",      "Copyright \xc2\xa9 2006-2023 The Xfce development team",
                         "authors",        auth,
                         NULL);
}
