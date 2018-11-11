#include <math.h>

#include "node.h"

#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

typedef struct
{
  UiNode *node;
  gdouble x;
  gdouble y;
  gdouble angle;
} UiNodeSocket;

struct _UiNodePrivate
{
  GList *sources;
  guint source_count;
  GList *sinks;
  guint sink_count;
  gboolean is_on_rim;
  gdouble pointer_angle;
  UiNodeStyle style;
};

enum {
  RIM_PRESSED,
  NUM_SIGNALS
};

/* Widget class overrides.  */
static void ui_node_size_allocate (GtkWidget *, GtkAllocation *);
static void ui_node_get_preferred_width (GtkWidget *, gint *, gint *);
static void ui_node_get_preferred_height  (GtkWidget *, gint *, gint *);
static void ui_node_get_preferred_width_for_height  (GtkWidget *, gint, gint *, gint *);
static void ui_node_get_preferred_height_for_width  (GtkWidget *, gint, gint *, gint *);
static gboolean ui_node_draw (GtkWidget *, cairo_t *);

/* Element class overrides.  */
static gboolean ui_node_contains_point (UiElement *, gdouble, gdouble);
static void ui_node_get_event_region (UiElement *, cairo_region_t *);

/* Signal handlers.  */
static gboolean on_button_press (GtkWidget *, GdkEventButton *, gpointer);
static gboolean on_motion_notify (GtkWidget *, GdkEventMotion *, gpointer);
static void on_hover_end (UiElement *);

static guint node_signals[NUM_SIGNALS] = {0};

G_DEFINE_TYPE_WITH_PRIVATE (UiNode, ui_node, UI_TYPE_ELEMENT)

static void
ui_node_class_init (UiNodeClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  UiElementClass *element_class = UI_ELEMENT_CLASS (class);

  widget_class->size_allocate = ui_node_size_allocate;
  widget_class->get_preferred_width = ui_node_get_preferred_width;
  widget_class->get_preferred_height = ui_node_get_preferred_height;
  widget_class->get_preferred_width_for_height = ui_node_get_preferred_width_for_height;
  widget_class->get_preferred_height_for_width = ui_node_get_preferred_height_for_width;
  widget_class->draw = ui_node_draw;
  gtk_widget_class_set_css_name (widget_class, "fonograf-node");

  element_class->contains_point = ui_node_contains_point;
  element_class->get_event_region = ui_node_get_event_region;

  node_signals[RIM_PRESSED] = g_signal_new ("rim-pressed",
                                            G_OBJECT_CLASS_TYPE (class),
                                            G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                            G_STRUCT_OFFSET (UiNodeClass, rim_pressed),
                                            NULL, NULL,
                                            NULL,
                                            G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}

static void
ui_node_init (UiNode *node)
{
  UiNodePrivate *priv;

  node->priv = ui_node_get_instance_private (node);
  priv = node->priv;
  priv->sources = NULL; /* @Leak: Source sockets are never freed.  */
  priv->source_count = 0;
  priv->sinks = NULL; /* @Leak: Sink sockets are never freed.  */
  priv->sink_count = 0;
  priv->is_on_rim = FALSE;
  priv->pointer_angle = 0.;
  priv->style = (UiNodeStyle) {
    .bg = (GdkRGBA) {.red = 1., .green = 1., .blue = 1., .alpha = 1.},
    .source_radius = 1.,
    .sink_radius = 1.,
    .source_margin = 0.,
    .sink_margin = 0.,
    .outline_width = 0.
  };

  g_signal_connect (GTK_WIDGET (node), "button-press-event",
                    G_CALLBACK (on_button_press), NULL);
  g_signal_connect (GTK_WIDGET (node), "motion-notify-event",
                    G_CALLBACK (on_motion_notify), NULL);
  g_signal_connect (GTK_WIDGET (node), "hover-end",
                    G_CALLBACK (on_hover_end), NULL);
}

static UiNodeSocket *
find_socket_by_node (GList *sockets, const UiNode *node)
{
  while (sockets)
    {
      UiNodeSocket *socket = sockets->data;
      if (socket->node == node)
        return socket;
      sockets = sockets->next;
    }
  return NULL;
}

static UiNodeSocket *
find_source_socket (const UiNode *node, const UiNode *source)
{
  return find_socket_by_node (node->priv->sources, source);
}

static UiNodeSocket *
find_sink_socket (const UiNode *node, const UiNode *sink)
{
  return find_socket_by_node (node->priv->sinks, sink);
}

static gint
cmp_source_angle (const UiNodeSocket *lhs, const UiNodeSocket *rhs,
                  UiNode *node)
{
  GtkAllocation n_alloc, lhs_alloc, rhs_alloc;
  gdouble lhs_angle = M_PI / 2, rhs_angle = M_PI / 2;

  gtk_widget_get_allocation (GTK_WIDGET (node), &n_alloc);

  if (lhs->node != NULL)
    {
      gtk_widget_get_allocation (GTK_WIDGET (lhs->node), &lhs_alloc);
      lhs_angle = atan2 (n_alloc.y - lhs_alloc.y, n_alloc.x - lhs_alloc.x);
    }

  if (rhs->node != NULL)
    {
      gtk_widget_get_allocation (GTK_WIDGET (rhs->node), &rhs_alloc);
      rhs_angle = atan2 (n_alloc.y - rhs_alloc.y, n_alloc.x - rhs_alloc.x);
    }

  /* Translate angle to have 0 pointing upward.  */
  lhs_angle -= M_PI / 2;
  rhs_angle -= M_PI / 2;
  if (lhs_angle <= -M_PI)
    lhs_angle += M_PI * 2;
  if (rhs_angle <= -M_PI)
    rhs_angle += M_PI * 2;

  if (lhs_angle < rhs_angle)
    return -1;
  else if (lhs_angle > rhs_angle)
    return 1;
  return 0;
}

static gint
cmp_sink_angle (const UiNodeSocket *lhs, const UiNodeSocket *rhs,
                UiNode *node)
{
  GtkAllocation n_alloc, lhs_alloc, rhs_alloc;
  gdouble lhs_angle = -M_PI / 2, rhs_angle = -M_PI / 2;

  gtk_widget_get_allocation (GTK_WIDGET (node), &n_alloc);

  if (lhs->node != NULL)
    {
      gtk_widget_get_allocation (GTK_WIDGET (lhs->node), &lhs_alloc);
      lhs_angle = atan2 (n_alloc.y - lhs_alloc.y, n_alloc.x - lhs_alloc.x);
    }

  if (rhs->node != NULL)
    {
      gtk_widget_get_allocation (GTK_WIDGET (rhs->node), &rhs_alloc);
      rhs_angle = atan2 (n_alloc.y - rhs_alloc.y, n_alloc.x - rhs_alloc.x);
    }

  /* Translate angle to have 0 pointing downward.  */
  lhs_angle += M_PI / 2;
  rhs_angle += M_PI / 2;
  if (lhs_angle >= M_PI)
    lhs_angle -= M_PI * 2;
  if (rhs_angle >= M_PI)
    rhs_angle -= M_PI * 2;

  if (lhs_angle < rhs_angle)
    return -1;
  else if (lhs_angle > rhs_angle)
    return 1;
  return 0;
}

static void
update_socket_coords (UiNode *node)
{
  UiNodePrivate *priv = node->priv;
  UiNodeStyle *style = &node->priv->style;
  GtkAllocation allocation;
  GList *sources;
  GList *sinks;
  gdouble nx, ny, nr;
  gdouble sr, sd;
  gdouble i;

  gtk_widget_get_allocation (GTK_WIDGET (node), &allocation);

  priv->sources = g_list_sort_with_data (priv->sources,
                                         (GCompareDataFunc) cmp_source_angle,
                                         node);
  priv->sinks = g_list_sort_with_data (priv->sinks,
                                       (GCompareDataFunc) cmp_sink_angle,
                                       node);

  nx = (gdouble) allocation.width / 2.;
  ny = (gdouble) allocation.height / 2.;
  nr = MIN (nx, ny) - style->outline_width;

  sr = style->source_radius + style->source_margin;
  sd = sqrt ((nr * nr) - (sr * sr));

  i = .5;
  for (sources = priv->sources; sources; sources = sources->next)
    {
      UiNodeSocket *socket = sources->data;
      gdouble a = i * (M_PI / priv->source_count) + M_PI;
      socket->x = (gdouble) allocation.x + nx + (cos (a) * sd);
      socket->y = (gdouble) allocation.y + ny + (sin (a) * sd);
      socket->angle = a;
      i += 1.;
    }

  sr = style->sink_radius + style->sink_margin;
  sd = sqrt ((nr * nr) - (sr * sr));

  i = .5;
  for (sinks = priv->sinks; sinks; sinks = sinks->next)
    {
      UiNodeSocket *socket = sinks->data;
      gdouble a = i * (M_PI / priv->sink_count);
      socket->x = (gdouble) allocation.x + nx + (cos (a) * sd);
      socket->y = (gdouble) allocation.y + ny + (sin (a) * sd);
      socket->angle = a;
      i += 1.;
    }
}

static void
update_style (UiNode *node)
{
  UiNodeStyle *style = &node->priv->style;
  GtkStyleContext *context = gtk_widget_get_style_context (GTK_WIDGET (node));
  GtkStateFlags flags = gtk_widget_get_state_flags (GTK_WIDGET (node));

  GdkRGBA *bg;
  gint source_radius = 1;
  gint sink_radius = 1;
  gint source_margin = 0;
  gint sink_margin = 0;
  gint outline_width = 0;

  gtk_style_context_get (context, flags,
                         "background-color", &bg,
                         "padding-top", &source_radius, /* @Hack */
                         "padding-bottom", &sink_radius, /* @Hack */
                         "margin-top", &source_margin, /* @Hack */
                         "margin-bottom", &sink_margin, /* @Hack */
                         "outline-width", &outline_width,
                         NULL);

  style->bg = *bg;
  style->source_radius = MAX (1., (gdouble) source_radius);
  style->sink_radius = MAX (1., (gdouble) sink_radius);
  style->source_margin = MAX (0., (gdouble) source_margin);
  style->sink_margin = MAX (0., (gdouble) sink_margin);
  style->outline_width = MAX (0., (gdouble) outline_width);

  gdk_rgba_free (bg);
}

/******************************************************************************/
/* API                                                                        */
/******************************************************************************/

GtkWidget *
ui_node_new ()
{
  return g_object_new (UI_TYPE_NODE, NULL);
}

GtkWidget *
ui_node_new_with_label (const gchar *label)
{
  GtkWidget *widget = g_object_new (UI_TYPE_NODE, NULL);
  GtkWidget *child = gtk_label_new (label);
  gtk_container_add (GTK_CONTAINER (widget), child);
  gtk_widget_show (child);
  return widget;
}

gboolean
ui_node_add_source (UiNode *node, UiNode *source)
{
  UiNodePrivate *priv;
  UiNodeSocket *socket;

  g_return_val_if_fail (UI_IS_NODE (node), FALSE);
  g_return_val_if_fail (UI_IS_NODE (source) || source == NULL, FALSE);
  g_return_val_if_fail (node != source, FALSE);
  /* @Incomplete: Check if we already have a socket for this source.  */

  priv = node->priv;

  socket = g_new (UiNodeSocket, 1);
  socket->node = source;
  socket->x = 0.;
  socket->y = 0.;

  priv->sources = g_list_append (priv->sources, socket);
  ++priv->source_count;

  return TRUE;
}

gboolean
ui_node_add_sink (UiNode *node, UiNode *sink)
{
  UiNodePrivate *priv;
  UiNodeSocket *socket;

  g_return_val_if_fail (UI_IS_NODE (node), FALSE);
  g_return_val_if_fail (UI_IS_NODE (sink) || sink == NULL, FALSE);
  g_return_val_if_fail (node != sink, FALSE);
  /* @Incomplete: Check if we already have a socket for this sink.  */

  priv = node->priv;

  socket = g_new (UiNodeSocket, 1);
  socket->node = sink;
  socket->x = 0.;
  socket->y = 0.;

  priv->sinks = g_list_append (priv->sinks, socket);
  ++priv->sink_count;

  return TRUE;
}

gboolean
ui_node_remove_source (UiNode *node, UiNode *source)
{
  UiNodePrivate *priv;
  UiNodeSocket *socket;
  gboolean removed = FALSE;

  g_return_val_if_fail (UI_IS_NODE (node), FALSE);
  g_return_val_if_fail (UI_IS_NODE (source) || source == NULL, FALSE);
  priv = node->priv;

  while ((socket = find_source_socket (node, source)) != NULL)
    {
      priv->sources = g_list_remove (priv->sources, socket);
      --priv->source_count;
      g_free (socket);
      removed = TRUE;
    }

  return removed;
}

gboolean
ui_node_remove_sink (UiNode *node, UiNode *sink)
{
  UiNodePrivate *priv;
  UiNodeSocket *socket;
  gboolean removed = FALSE;

  g_return_val_if_fail (UI_IS_NODE (node), FALSE);
  g_return_val_if_fail (UI_IS_NODE (sink) || sink == NULL, FALSE);
  priv = node->priv;

  while ((socket = find_sink_socket (node, sink)) != NULL)
    {
      priv->sinks = g_list_remove (priv->sinks, socket);
      --priv->sink_count;
      g_free (socket);
      removed = TRUE;
    }

  return removed;
}

gboolean
ui_node_get_source_socket (const UiNode *node,
                           const UiNode *source,
                           UiSocket *socket)
{
  UiNodeSocket *node_socket;

  g_return_val_if_fail (UI_IS_NODE (node), FALSE);
  g_return_val_if_fail (UI_IS_NODE (source) || source == NULL, FALSE);
  g_return_val_if_fail (socket != NULL, FALSE);

  node_socket = find_source_socket (node, source);
  if (node_socket == NULL)
    return FALSE;

  socket->x = node_socket->x;
  socket->y = node_socket->y;
  socket->radius = node->priv->style.source_radius;
  socket->angle = node_socket->angle;

  return TRUE;
}

gboolean
ui_node_get_sink_socket (const UiNode *node,
                         const UiNode *sink,
                         UiSocket *socket)
{
  UiNodeSocket *node_socket;

  g_return_val_if_fail (UI_IS_NODE (node), FALSE);
  g_return_val_if_fail (UI_IS_NODE (sink) || sink == NULL, FALSE);
  g_return_val_if_fail (socket != NULL, FALSE);

  node_socket = find_sink_socket (node, sink);
  if (node_socket == NULL)
    return FALSE;

  socket->x = node_socket->x;
  socket->y = node_socket->y;
  socket->radius = node->priv->style.sink_radius;
  socket->angle = node_socket->angle;

  return TRUE;
}

const UiNodeStyle *
ui_node_get_style (const UiNode *node)
{
  g_return_val_if_fail (UI_IS_NODE (node), NULL);
  return &node->priv->style;
}

/******************************************************************************/
/* GtkWidget Overrides                                                        */
/******************************************************************************/

static void
ui_node_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
  UiNode *node = UI_NODE (widget);
  update_style (node);

  GTK_WIDGET_CLASS (ui_node_parent_class)->size_allocate (widget, allocation);

  update_socket_coords (node);
}

static void
ui_node_get_preferred_width (GtkWidget *widget,
                             gint *minimum_size,
                             gint *natural_size)
{
  GtkStyleContext *context = gtk_widget_get_style_context (widget);
  GtkStateFlags flags = gtk_widget_get_state_flags (widget);
  gint min_width;
  gint min_height;
  gint outline_width;

  gtk_style_context_get (context, flags,
                         "min-width", &min_width,
                         "min-height", &min_height,
                         "outline-width", &outline_width,
                         NULL);

  *minimum_size = MAX (50, MAX (min_width, min_height)) + (outline_width * 2);
  *natural_size = *minimum_size;
}

static void
ui_node_get_preferred_height  (GtkWidget *widget,
                               gint *minimum_size,
                               gint *natural_size)
{
  ui_node_get_preferred_width (widget, minimum_size, natural_size);
}

static void
ui_node_get_preferred_width_for_height  (GtkWidget *widget,
                                         gint width,
                                         gint *minimum_height,
                                         gint *natural_height)
{
  (void) widget;
  *minimum_height = width;
  *natural_height = width;
}

static void
ui_node_get_preferred_height_for_width  (GtkWidget *widget,
                                         gint height,
                                         gint *minimum_width,
                                         gint *natural_width)
{
  (void) widget;
  *minimum_width = height;
  *natural_width = height;
}

static gboolean
ui_node_draw (GtkWidget *widget, cairo_t *cr)
{
  UiNode *node = UI_NODE (widget);
  UiNodePrivate *priv;
  UiNodeStyle *style;
  GtkAllocation allocation;
  gdouble nx;
  gdouble ny;
  gdouble nr;
  gdouble sr; /* Socket radius (+ margin) */
  gdouble sd; /* Distance to center of socket */
  gdouble sra; /* Socket radius angle */
  gdouble pa; /* Previous angle */
  GdkRGBA *bg;

  update_style (node);
  priv = node->priv;
  style = &priv->style;
  bg = &style->bg;

  gtk_widget_get_allocation (widget, &allocation);
  nx = (gdouble) allocation.width / 2.;
  ny = (gdouble) allocation.height / 2.;
  nr = MIN (nx, ny) - style->outline_width;

  if (priv->is_on_rim)
    /* Needed for CLEAR operator to have effect, see below.  */
    cairo_push_group (cr);

  if (gtk_widget_has_visible_focus (widget))
    {
      /* Draw halo.  */
      cairo_set_source_rgba (cr, bg->red, bg->green, bg->blue, bg->alpha * .25);
      cairo_arc (cr, nx, ny, nr + style->outline_width, 0, M_PI * 2);
      cairo_fill (cr);
    }

  cairo_set_source_rgba (cr, bg->red, bg->green, bg->blue, bg->alpha);

  sr = style->sink_radius + style->sink_margin;
  sd = sqrt ((nr * nr) - (sr * sr));
  sra = atan (sr / sd);

  /* Trace lower half of node with sink insets.  */
  pa = 0.;
  for (guint i = 0; i < priv->sink_count; ++i)
    {
      gdouble a = (M_PI / priv->sink_count) * ((gdouble) i + .5);
      gdouble xu = cos (a);
      gdouble yu = sin (a);
      gdouble sx = nx + (xu * sd);
      gdouble sy = ny + (yu * sd);

      /* Trace node rim.  */
      cairo_arc (cr, nx, ny, nr, pa, a - sra);
      /* Trace socket inset.  */
      cairo_arc_negative (cr, sx, sy, sr, a - M_PI / 2, a - M_PI * 1.5);
      pa = a + sra; /* Record angle at edge of socket.  */
    }
  cairo_arc (cr, nx, ny, nr, pa, M_PI);

  sr = style->source_radius + style->source_margin;
  sd = sqrt ((nr * nr) - (sr * sr));
  sra = atan (sr / sd);

  /* Trace upper half of node with source insets.  */
  pa = M_PI;
  for (guint i = 0; i < priv->source_count; ++i)
    {
      gdouble a = (M_PI / priv->source_count) * ((gdouble) i + .5) + M_PI;
      gdouble xu = cos (a);
      gdouble yu = sin (a);
      gdouble sx = nx + (xu * sd);
      gdouble sy = ny + (yu * sd);

      /* Trace node rim.  */
      cairo_arc (cr, nx, ny, nr, pa, a - sra);
      /* Trace socket inset.  */
      cairo_arc_negative (cr, sx, sy, sr, a - M_PI / 2, a - M_PI * 1.5);
      pa = a + sra; /* Record angle at edge of socket.  */
    }
  cairo_arc (cr, nx, ny, nr, pa, 2 * M_PI);

  cairo_fill (cr);

  if (priv->is_on_rim)
    {
      /* If mouse pointer is on the rim of the node, we draw something
         indicating a possibiliy for a new connection.  */
      gdouble xu = cos (priv->pointer_angle);
      gdouble yu = sin (priv->pointer_angle);
      gdouble radius = (yu < 0.) ? style->source_radius : style->sink_radius;
      gdouble margin = (yu < 0.) ? style->source_margin : style->sink_margin;
      gdouble sx = nx + (xu * (nr - radius));
      gdouble sy = ny + (yu * (nr - radius));

      cairo_arc (cr, sx, sy, radius + margin, 0, M_PI * 2);
      cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
      cairo_fill (cr);
      cairo_pop_group_to_source (cr);
      cairo_paint (cr);

      cairo_set_source_rgba (cr, bg->red, bg->green, bg->blue, bg->alpha);
      cairo_arc (cr, sx, sy, radius, 0, M_PI * 2);
      cairo_fill (cr);
    }

  GTK_WIDGET_CLASS (ui_node_parent_class)->draw (widget, cr);

  return FALSE;
}

/******************************************************************************/
/* UiElement Overrides                                                      */
/******************************************************************************/

static gboolean
ui_node_contains_point (UiElement *element, gdouble x, gdouble y)
{
  UiNode *node = UI_NODE (element);
  UiNodePrivate *priv;
  GtkAllocation allocation;
  gdouble nx, ny, nr, px, py, pr, sr;
  GList *sources;
  GList *sinks;

  priv = node->priv;
  gtk_widget_get_allocation (GTK_WIDGET (node), &allocation);
  nx = (gdouble) allocation.width / 2.;
  ny = (gdouble) allocation.height / 2.;
  nr = MIN (nx, ny) - (gdouble) priv->style.outline_width;
  px = x - nx;
  py = y - ny;
  pr = sqrt (px * px + py * py);

  if (nr < pr)
    return FALSE; /* Point is outside main circle.  */

  sr = priv->style.source_radius + priv->style.source_margin;

  for (sources = priv->sources; sources; sources = sources->next)
    {
      UiNodeSocket *socket = sources->data;
      gdouble sx = socket->x - (gdouble) allocation.x;
      gdouble sy = socket->y - (gdouble) allocation.y;
      px = x - sx;
      py = y - sy;
      pr = sqrt (px * px + py * py);

      if (pr < sr)
        return FALSE; /* Point is inside source socket.  */
    }

  sr = priv->style.sink_radius + priv->style.sink_margin;

  for (sinks = priv->sinks; sinks; sinks = sinks->next)
    {
      UiNodeSocket *socket = sinks->data;
      gdouble sx = socket->x - (gdouble) allocation.x;
      gdouble sy = socket->y - (gdouble) allocation.y;
      px = x - sx;
      py = y - sy;
      pr = sqrt (px * px + py * py);

      if (pr < sr)
        return FALSE; /* Point is inside sink socket.  */
    }

  return TRUE;
}

static void
ui_node_get_event_region (UiElement *element, cairo_region_t *region)
{
  UiNode *node;
  UiNodePrivate *priv;
  GtkAllocation allocation;
  GList *sources;
  GList *sinks;
  cairo_rectangle_int_t rect;
  gdouble socket_diameter;
  int socket_region_width;

  node = UI_NODE (element);
  priv = node->priv;
  gtk_widget_get_allocation (GTK_WIDGET (node), &allocation);

  rect.x = (int) priv->style.outline_width;
  rect.y = (int) priv->style.outline_width;
  rect.width = allocation.width - (rect.x * 2);
  rect.height = allocation.height - (rect.y * 2);

  cairo_region_union_rectangle (region, &rect);

  socket_diameter = (priv->style.source_radius + priv->style.source_margin) * 2;
  socket_region_width = sqrt ((socket_diameter * socket_diameter) / 2.);

  for (sources = priv->sources; sources; sources = sources->next)
    {
      UiNodeSocket *socket = sources->data;
      rect.x = (int) socket->x - allocation.x - (socket_region_width / 2);
      rect.y = (int) socket->y - allocation.y - (socket_region_width / 2);
      rect.width = socket_region_width;
      rect.height = socket_region_width;
      cairo_region_subtract_rectangle (region, &rect);
    }

  socket_diameter = (priv->style.sink_radius + priv->style.sink_margin) * 2;
  socket_region_width = sqrt ((socket_diameter * socket_diameter) / 2.);

  for (sinks = priv->sinks; sinks; sinks = sinks->next)
    {
      UiNodeSocket *socket = sinks->data;
      rect.x = (int) socket->x - allocation.x - (socket_region_width / 2);
      rect.y = (int) socket->y - allocation.y - (socket_region_width / 2);
      rect.width = socket_region_width;
      rect.height = socket_region_width;
      cairo_region_subtract_rectangle (region, &rect);
    }
}

/******************************************************************************/
/* Signal Handlers                                                            */
/******************************************************************************/

static gboolean
on_button_press (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
  (void) user_data;
  (void) event;

  UiNode *node = UI_NODE (widget);

  if (node->priv->is_on_rim)
    g_signal_emit (node, node_signals[RIM_PRESSED], 0,
                   (node->priv->pointer_angle < 0.) ? TRUE : FALSE);

  return FALSE;
}

static gboolean
on_motion_notify (GtkWidget *widget, GdkEventMotion *event, gpointer user_data)
{
  (void) user_data;

  UiNode *node = UI_NODE (widget);
  UiNodePrivate *priv;
  GtkAllocation allocation;
  gdouble nx, ny, nr, px, py, pr, sr;

  priv = node->priv;
  gtk_widget_get_allocation (GTK_WIDGET (node), &allocation);
  nx = (gdouble) allocation.width / 2.;
  ny = (gdouble) allocation.height / 2.;
  nr = MIN (nx, ny) - priv->style.outline_width;
  px = event->x - nx;
  py = event->y - ny;
  pr = sqrt (px * px + py * py);
  sr = (py < 0.) ? priv->style.source_radius : priv->style.sink_radius;

  if (pr > nr || pr < nr - sr * 2)
    {
      if (priv->is_on_rim)
        gtk_widget_queue_draw (widget);
      priv->is_on_rim = FALSE;
    }
  else
    {
      priv->is_on_rim = TRUE;
      priv->pointer_angle = atan2 (py, px);
      gtk_widget_queue_draw (widget);
    }

  return FALSE;
}

static void
on_hover_end (UiElement *element)
{
  UiNode *node = UI_NODE (element);
  UiNodePrivate *priv = node->priv;

  if (priv->is_on_rim)
    gtk_widget_queue_draw (GTK_WIDGET (element));

  priv->is_on_rim = FALSE;
}
