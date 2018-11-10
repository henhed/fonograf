#include <stdlib.h>

#include <lv2/lv2plug.in/ns/lv2core/lv2.h>
#include <lv2/lv2plug.in/ns/ext/log/log.h>
#include <lv2/lv2plug.in/ns/ext/log/logger.h>
#include <lv2/lv2plug.in/ns/extensions/ui/ui.h>

typedef struct _FonoGrafUI {
  LV2_Log_Log *log;
  LV2_Log_Logger logger;
  LV2_URID_Map *map;
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
  (void) widget;

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

  lv2_log_error (&ui->logger,
                 __FILE__ ":instantiate(%s, %s) not implemented\n",
                 plugin_uri, bundle_path);

  return ui;
}

/* Free any resources allocated in `instantiate'.  */
static void
cleanup (LV2UI_Handle handle)
{
  FonoGrafUI *ui = (FonoGrafUI *) handle;
  if (!ui)
    return;

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
