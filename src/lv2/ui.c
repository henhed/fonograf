#include <stdlib.h>

#include <lv2/lv2plug.in/ns/lv2core/lv2.h>
#include <lv2/lv2plug.in/ns/ext/log/log.h>
#include <lv2/lv2plug.in/ns/ext/log/logger.h>
#include <lv2/lv2plug.in/ns/extensions/ui/ui.h>

#include "../gtk3/graph.h"
#include "../gtk3/node.h"

typedef struct _FonoGrafUI {
  LV2_Log_Log *log;
  LV2_Log_Logger logger;
  LV2_URID_Map *map;
  GtkWidget *root;
} FonoGrafUI;

/* Set up UI.  */
static LV2UI_Handle
instantiate (const LV2UI_Descriptor *descriptor, const char *plugin_uri,
             const char *bundle_path, LV2UI_Write_Function write_function,
             LV2UI_Controller controller, LV2UI_Widget *widget,
             const LV2_Feature * const *features)
{
  FonoGrafUI *ui;

  (void) descriptor;
  (void) plugin_uri;
  (void) bundle_path;
  (void) write_function;
  (void) controller;

  ui = calloc (1, sizeof (FonoGrafUI));
  if (!ui)
    return NULL;

  for (uint32_t i = 0; features[i]; ++i)
    if (!strcmp (features[i]->URI, LV2_URID__map))
      ui->map = (LV2_URID_Map *) features[i]->data;
    else if (!strcmp (features[i]->URI, LV2_LOG__log))
      ui->log = (LV2_Log_Log *) features[i]->data;

  lv2_log_logger_init (&ui->logger, ui->map, ui->log);

  if (!ui->map)
    {
      lv2_log_error (&ui->logger, "Missing feature uri:map\n");
      free (ui);
      return NULL;
    }

  ui->root = ui_graph_new ();

  /* Load custom CSS.  */
  {
    GtkCssProvider *style_provider = gtk_css_provider_new ();
    gchar *css_filename = g_build_path ("/", bundle_path, "ui.css", NULL);
    GError *error = NULL;

    gtk_css_provider_load_from_path (style_provider, css_filename, &error);
    if (error != NULL)
      lv2_log_warning (&ui->logger, __FILE__ ": %s\n", error->message);
    else
      {
        GdkScreen *screen = gdk_display_get_default_screen (gdk_display_get_default ());
        gtk_style_context_add_provider_for_screen (screen,
                                                   GTK_STYLE_PROVIDER (style_provider),
                                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
      }

    g_free (css_filename);
    g_object_unref (style_provider);
  }

  *widget = (LV2UI_Widget) ui->root;

  /* @Temporary: Sample UI graph */
  {
    GtkWidget *node1 = ui_node_new_with_label ("Test #1");
    GtkWidget *node2 = ui_node_new_with_label ("Test #2");
    GtkWidget *node3 = ui_node_new_with_label ("Test #3");

    ui_graph_put (UI_GRAPH (ui->root), UI_NODE (node1), .50, .25);
    ui_graph_put (UI_GRAPH (ui->root), UI_NODE (node2), .25, .50);
    ui_graph_put (UI_GRAPH (ui->root), UI_NODE (node3), .75, .75);

    ui_graph_connect (UI_GRAPH (ui->root), UI_NODE (node1), UI_NODE (node2));
    ui_graph_connect (UI_GRAPH (ui->root), UI_NODE (node1), UI_NODE (node3));
    ui_graph_connect (UI_GRAPH (ui->root), UI_NODE (node2), UI_NODE (node3));
  }

  return ui;
}

/* Free any resources allocated in `instantiate'.  */
static void
cleanup (LV2UI_Handle handle)
{
  FonoGrafUI *ui = (FonoGrafUI *) handle;
  if (!ui)
    return;

  gtk_widget_destroy (ui->root);

  free (ui);
}

/* Port event listener.  */
static void
port_event (LV2UI_Handle handle, uint32_t port_index, uint32_t buffer_size,
            uint32_t format, const void *buffer)
{
  FonoGrafUI *ui = (FonoGrafUI *) handle;

  lv2_log_error (&ui->logger,
                 __FILE__ ":port_event(%u, %u, %u, %p) not implemented\n",
                 port_index, buffer_size, format, buffer);
}

/* Return any extension data supported by this UI.  */
static const void *
extension_data (const char *uri)
{
  (void) uri;
  return NULL;
}

/* FonoGraf UI descriptor.  */
static const LV2UI_Descriptor descriptor = {
  "http://www.henhed.se/lv2/fonograf#ui",
  instantiate,
  cleanup,
  port_event,
  extension_data
};

/* Export UI to lv2 host.  */
LV2_SYMBOL_EXPORT
const LV2UI_Descriptor *
lv2ui_descriptor (uint32_t index)
{
  if (index == 0)
    return &descriptor;
  return NULL;
}
