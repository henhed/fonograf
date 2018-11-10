#include <stdlib.h>

#include <lv2/lv2plug.in/ns/lv2core/lv2.h>
#include <lv2/lv2plug.in/ns/ext/log/log.h>
#include <lv2/lv2plug.in/ns/ext/log/logger.h>

typedef struct _FonoGraf {
  LV2_Log_Log *log;
  LV2_Log_Logger logger;
  LV2_URID_Map *map;
} FonoGraf;

/* Set up plugin.  */
static LV2_Handle
instantiate (const LV2_Descriptor *descriptor, double rate,
             const char *bundle_path,
             const LV2_Feature * const *features)
{
  FonoGraf *plugin;

  (void) descriptor;
  (void) rate;
  (void) bundle_path;

  plugin = calloc (1, sizeof (FonoGraf));
  if (!plugin)
    return NULL;

  for (uint32_t i = 0; features[i]; ++i)
    if (!strcmp (features[i]->URI, LV2_URID__map))
      plugin->map = (LV2_URID_Map *) features[i]->data;
    else if (!strcmp (features[i]->URI, LV2_LOG__log))
      plugin->log = (LV2_Log_Log *) features[i]->data;

  lv2_log_logger_init (&plugin->logger, plugin->map, plugin->log);

  if (!plugin->map)
    {
      lv2_log_error (&plugin->logger, "Missing feature uri:map\n");
      free (plugin);
      return NULL;
    }

  return plugin;
}

/* Connect host ports to the plugin instance. Port buffers may only be
   accessed by functions in the same (`audio') threading class.  */
static void
connect_port (LV2_Handle instance, uint32_t port, void *data)
{
  FonoGraf *plugin = (FonoGraf *) instance;

  (void) port;
  (void) data;

  lv2_log_error (&plugin->logger,
                 __FILE__ ":connect_port(%u, %p) not implemented\n",
                 port, data);
}

/* Write NFRAMES frames to audio output ports. This function runs in
   in the `audio' threading class and must be real-time safe.  */
static void
run (LV2_Handle instance, uint32_t nframes)
{
  FonoGraf *plugin = (FonoGraf *) instance;

  (void) plugin;
  (void) nframes;

  lv2_log_error (&plugin->logger,
                 __FILE__ ":run(%u) not implemented\n",
                 nframes);
}

/* Free any resources allocated in `instantiate'.  */
static void
cleanup (LV2_Handle instance)
{
  FonoGraf *plugin = (FonoGraf *) instance;
  if (!plugin)
    return;

  free (plugin);
}

/* Return any extension data supported by this plugin.  */
static const void *
extension_data (const char *uri)
{
  (void) uri;
  return NULL;
}

/* FonoGraf plugin descriptor.  */
static const LV2_Descriptor descriptor = {
  "http://www.henhed.se/lv2/fonograf",
  instantiate,
  connect_port,
  NULL,
  run,
  NULL,
  cleanup,
  extension_data
};

/* Export plugin to lv2 host.  */
LV2_SYMBOL_EXPORT
const LV2_Descriptor *
lv2_descriptor (uint32_t index)
{
  if (index == 0)
    return &descriptor;
  return NULL;
}
