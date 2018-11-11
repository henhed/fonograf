#include <math.h>

#include "edge.h"

#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

/* @HardCode: Distance from socket to Bezier control point.  */
#define SOCKET_EXTRUSION 128.

struct _UiEdgePrivate
{
  UiNode *source;
  UiNode *sink;
  gdouble weight;
  UiNode *detached;
  gdouble detached_x;
  gdouble detached_y;
  gdouble anim_phase;
  guint anim_thread_id;
};

enum {
  ATTACHED,
  DETACHED,
  NUM_SIGNALS
};

/* GObject class overrides.  */
static void ui_edge_dispose (GObject *);

/* Widget class overrides.  */
static void ui_edge_size_allocate (GtkWidget *, GtkAllocation *);
static gboolean ui_edge_draw (GtkWidget *, cairo_t *);

/* Element class overrides.  */
static gboolean ui_edge_contains_point (UiElement *, gdouble, gdouble);
static void ui_edge_get_event_region (UiElement *, cairo_region_t *);

/* Signal handlers.  */
static gboolean on_anim_timeout (gpointer);

/* Helper functions.  */
static gdouble get_curve_point_1d (gdouble, gdouble, gdouble, gdouble, gdouble);
static void get_bounds_for_curve (gdouble, gdouble, gdouble, gdouble, gdouble,
                                  gdouble, gdouble, gdouble, GdkRectangle *);

static guint edge_signals[NUM_SIGNALS] = {0};

G_DEFINE_TYPE_WITH_PRIVATE (UiEdge, ui_edge, UI_TYPE_ELEMENT)

static void
ui_edge_class_init (UiEdgeClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  UiElementClass *element_class = UI_ELEMENT_CLASS (class);

  gobject_class->dispose = ui_edge_dispose;

  widget_class->size_allocate = ui_edge_size_allocate;
  widget_class->draw = ui_edge_draw;
  gtk_widget_class_set_css_name (widget_class, "gf-edge");

  element_class->contains_point = ui_edge_contains_point;
  element_class->get_event_region = ui_edge_get_event_region;

  edge_signals[ATTACHED] = g_signal_new ("attached",
                                         G_OBJECT_CLASS_TYPE (class),
                                         G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                         G_STRUCT_OFFSET (UiEdgeClass, attached),
                                         NULL, NULL,
                                         NULL,
                                         G_TYPE_NONE,
                                         2, UI_TYPE_NODE, UI_TYPE_NODE);

  edge_signals[DETACHED] = g_signal_new ("detached",
                                         G_OBJECT_CLASS_TYPE (class),
                                         G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                         G_STRUCT_OFFSET (UiEdgeClass, detached),
                                         NULL, NULL,
                                         NULL,
                                         G_TYPE_NONE, 1, UI_TYPE_NODE);
}

static void
ui_edge_init (UiEdge *edge)
{
  UiEdgePrivate *priv;

  edge->priv = ui_edge_get_instance_private (edge);
  priv = edge->priv;

  priv->source = NULL;
  priv->sink = NULL;
  priv->weight = 1.;
  priv->detached = NULL;
  priv->detached_x = 0.;
  priv->detached_y = 0.;
  priv->anim_phase = 0.;
  priv->anim_thread_id = gdk_threads_add_timeout (50, on_anim_timeout, edge);
  g_source_set_name_by_id (priv->anim_thread_id, "[Graflow] on_anim_timeout");
}

/******************************************************************************/
/* API                                                                        */
/******************************************************************************/

GtkWidget *
ui_edge_new (UiNode *source, UiNode *sink)
{
  GtkWidget *edge;
  UiEdgePrivate *priv;

  g_return_val_if_fail (UI_IS_NODE (source), NULL);
  g_return_val_if_fail (UI_IS_NODE (sink), NULL);

  edge = g_object_new (UI_TYPE_EDGE, NULL);
  priv = UI_EDGE (edge)->priv;

  priv->source = source;
  priv->sink = sink;

  g_object_ref (priv->source);
  g_object_ref (priv->sink);

  return edge;
}

void
ui_edge_get_bounds (UiEdge *edge, GdkRectangle *bounds)
{
  UiEdgePrivate *priv;
  UiSocket source;
  UiSocket sink;
  gdouble source_ax, source_ay, sink_ax, sink_ay;
  gdouble max_radius;

  g_return_if_fail (UI_IS_EDGE (edge));
  g_return_if_fail (bounds != NULL);

  priv = edge->priv;

  ui_node_get_sink_socket (priv->source, priv->sink, &source);
  ui_node_get_source_socket (priv->sink, priv->source, &sink);

  if (priv->source == priv->detached)
    {
      source.x = priv->detached_x;
      source.y = priv->detached_y;
      source.angle = M_PI * .5;
    }
  else if (priv->sink == priv->detached)
    {
      sink.x = priv->detached_x;
      sink.y = priv->detached_y;
      sink.angle = M_PI * 1.5;
    }

  source_ax = source.x + (cos (source.angle) * SOCKET_EXTRUSION);
  source_ay = source.y + (sin (source.angle) * SOCKET_EXTRUSION);
  sink_ax = sink.x + (cos (sink.angle) * SOCKET_EXTRUSION);
  sink_ay = sink.y + (sin (sink.angle) * SOCKET_EXTRUSION);

  get_bounds_for_curve (source.x, source.y,
                        source_ax, source_ay,
                        sink_ax, sink_ay,
                        sink.x, sink.y,
                        bounds);

  /* Expand box to include sockets, meaning also that there is room for the edge
     to be as thick as the biggest socket.  */
  max_radius = MAX (source.radius, sink.radius);
  bounds->x -= max_radius;
  bounds->y -= max_radius;
  bounds->width += max_radius * 2;
  bounds->height += max_radius * 2;
}

UiNode *
ui_edge_get_source (UiEdge *edge)
{
  g_return_val_if_fail (UI_IS_EDGE (edge), NULL);
  return edge->priv->source;
}

UiNode *
ui_edge_get_sink (UiEdge *edge)
{
  g_return_val_if_fail (UI_IS_EDGE (edge), NULL);
  return edge->priv->sink;
}

void
ui_edge_set_weight (UiEdge *edge, gdouble weight)
{
  g_return_if_fail (UI_IS_EDGE (edge));
  g_return_if_fail (weight >= 0 && weight <= 1);
  edge->priv->weight = weight;
  gtk_widget_queue_draw (GTK_WIDGET (edge));
}

gboolean
ui_edge_detach (UiEdge *edge, UiNode *node)
{
  g_return_val_if_fail (UI_IS_EDGE (edge), FALSE);
  g_return_val_if_fail (UI_IS_NODE (node), FALSE);
  g_return_val_if_fail (node == edge->priv->source || node == edge->priv->sink, FALSE);

  edge->priv->detached = node;

  g_signal_emit (edge, edge_signals[DETACHED], 0, node);

  return TRUE;
}

UiNode *
ui_edge_get_detached (UiEdge *edge)
{
  g_return_val_if_fail (UI_IS_EDGE (edge), NULL);
  return edge->priv->detached;
}

void
ui_edge_set_detached_pos (UiEdge *edge, gdouble x, gdouble y)
{
  g_return_if_fail (UI_IS_EDGE (edge));
  edge->priv->detached_x = x;
  edge->priv->detached_y = y;
}

gboolean
ui_edge_attach (UiEdge *edge, UiNode *node)
{
  g_return_val_if_fail (UI_IS_EDGE (edge), FALSE);

  if (node == NULL)
    node = edge->priv->detached;

  g_signal_emit (edge, edge_signals[ATTACHED], 0, node, edge->priv->detached);

  edge->priv->detached = NULL;

  return TRUE;
}

/******************************************************************************/
/* GObject Overrides                                                          */
/******************************************************************************/

static void
ui_edge_dispose (GObject *object)
{
  UiEdge *edge = UI_EDGE (object);
  UiEdgePrivate *priv = edge->priv;

  if (priv->anim_thread_id > 0)
    {
      g_source_remove (priv->anim_thread_id);
      priv->anim_thread_id = 0;
    }

  ui_node_remove_sink (priv->source, priv->sink);
  ui_node_remove_source (priv->sink, priv->source);
  g_object_unref (priv->source);
  g_object_unref (priv->sink);

  G_OBJECT_CLASS (ui_edge_parent_class)->dispose (object);
}

/******************************************************************************/
/* GtkWidget Overrides                                                        */
/******************************************************************************/

static void
ui_edge_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
  GTK_WIDGET_CLASS (ui_edge_parent_class)->size_allocate (widget, allocation);

  if (gtk_widget_get_realized (widget))
    {
      UiEdge *edge;
      GtkPopover *popover;
      UiSocket source;
      UiSocket sink;
      GdkRectangle midpoint;
      gdouble x1, y1, x2, y2, x3, y3, x4, y4;

      edge = UI_EDGE (widget);
      popover = ui_element_get_popover (UI_ELEMENT (widget));
      ui_node_get_sink_socket (edge->priv->source, edge->priv->sink, &source);
      ui_node_get_source_socket (edge->priv->sink, edge->priv->source, &sink);

      x1 = (gdouble) source.x - allocation->x;
      y1 = (gdouble) source.y - allocation->y;
      x2 = x1 + (cos (source.angle) * SOCKET_EXTRUSION);
      y2 = y1 + (sin (source.angle) * SOCKET_EXTRUSION);
      x4 = (gdouble) sink.x - allocation->x;
      y4 = (gdouble) sink.y - allocation->y;
      x3 = x4 + (cos (sink.angle) * SOCKET_EXTRUSION);
      y3 = y4 + (sin (sink.angle) * SOCKET_EXTRUSION);

      midpoint.x = get_curve_point_1d (x1, x2, x3, x4, .5);
      midpoint.y = get_curve_point_1d (y1, y2, y3, y4, .5);
      midpoint.width = 1;
      midpoint.height = 1;

      gtk_popover_set_pointing_to (popover, &midpoint);
    }
}

static gboolean
ui_edge_draw (GtkWidget *widget, cairo_t *cr)
{
  UiEdge *edge = UI_EDGE (widget);
  UiEdgePrivate *priv = edge->priv;
  GtkAllocation allocation;
  UiSocket source;
  UiSocket sink;
  gdouble source_x, source_y, sink_x, sink_y;
  gdouble source_ax, source_ay, sink_ax, sink_ay;
  const GdkRGBA *bg;

  gtk_widget_get_allocation (widget, &allocation);
  ui_node_get_sink_socket (priv->source, priv->sink, &source);
  ui_node_get_source_socket (priv->sink, priv->source, &sink);

  if (priv->source == priv->detached)
    {
      source.x = priv->detached_x;
      source.y = priv->detached_y;
      source.angle = M_PI * .5;
    }
  else if (priv->sink == priv->detached)
    {
      sink.x = priv->detached_x;
      sink.y = priv->detached_y;
      sink.angle = M_PI * 1.5;
    }

  bg = &ui_node_get_style (priv->source)->bg;
  cairo_set_source_rgba (cr, bg->red, bg->green, bg->blue, bg->alpha);
  if (priv->weight == 0. || priv->detached != NULL)
    {
      gdouble avg = (bg->red + bg->green + bg->blue) / 3;
      cairo_set_source_rgba (cr, avg, avg, avg, bg->alpha);
    }
  else if (priv->weight < 1.)
    {
      double dashes[] = {
        SOCKET_EXTRUSION * priv->weight,
        SOCKET_EXTRUSION * (1. - priv->weight)
      };
      cairo_set_dash (cr, dashes, 2, -source.radius * .8);
    }

  cairo_set_line_cap (cr, CAIRO_LINE_CAP_BUTT);

  /* Translate socket coords.  */
  source_x = source.x - (gdouble) allocation.x;
  source_y = source.y - (gdouble) allocation.y;
  sink_x = sink.x - (gdouble) allocation.x;
  sink_y = sink.y - (gdouble) allocation.y;

  /* Bezier anchor points.  */
  source_ax = source_x + (cos (source.angle) * SOCKET_EXTRUSION);
  source_ay = source_y + (sin (source.angle) * SOCKET_EXTRUSION);
  sink_ax = sink_x + (cos (sink.angle) * SOCKET_EXTRUSION);
  sink_ay = sink_y + (sin (sink.angle) * SOCKET_EXTRUSION);

  /* Draw edge curve.  */
  cairo_move_to (cr, source_x, source_y);
  cairo_curve_to (cr, source_ax, source_ay, sink_ax, sink_ay, sink_x, sink_y);
  if (priv->weight > 0.)
    {
      cairo_set_line_width (cr, MIN (source.radius, sink.radius));
      if (priv->weight < 1.)
        cairo_stroke_preserve (cr);
      else
        cairo_stroke (cr);
    }

  if (priv->weight < 1.)
    {
      cairo_set_dash (cr, NULL, 0, 0);
      cairo_set_line_width (cr, MIN (source.radius, sink.radius) * .5);
      cairo_stroke (cr);
    }

  /* Draw source socket.  */
  cairo_arc (cr, source_x, source_y, source.radius, 0, M_PI * 2);
  cairo_fill (cr);

  /* Draw sink socket.  */
  cairo_arc (cr, sink_x, sink_y, sink.radius, 0, M_PI * 2);
  cairo_fill (cr);

  /* Draw direction animtaion.  */
  if (priv->weight > 0. && priv->detached == NULL)
    {
      gdouble phase = priv->anim_phase;
      gdouble phase2 = fabs ((phase * 2.) - 1.);
      bg = &ui_node_get_style (priv->sink)->bg;
      cairo_set_source_rgba (cr, bg->red, bg->green, bg->blue,
                             bg->alpha * phase2);

      cairo_arc (cr,
                 get_curve_point_1d (source_x, source_ax, sink_ax, sink_x,
                                     phase),
                 get_curve_point_1d (source_y, source_ay, sink_ay, sink_y,
                                     phase),
                 (source.radius * (1. - phase) + sink.radius * phase) * phase2,
                 0,
                 M_PI * 2);
      cairo_fill (cr);

      cairo_set_source_rgba (cr, bg->red, bg->green, bg->blue,
                             bg->alpha * phase);
      cairo_arc (cr, source_x, source_y, source.radius * phase, 0, M_PI * 2);
      cairo_fill (cr);
      cairo_set_source_rgba (cr, bg->red, bg->green, bg->blue,
                             bg->alpha * (1. - phase));
      cairo_arc (cr, sink_x, sink_y, sink.radius * (1. - phase), 0, M_PI * 2);
      cairo_fill (cr);
    }

  GTK_WIDGET_CLASS (ui_edge_parent_class)->draw (widget, cr);

  return FALSE;
}

/******************************************************************************/
/* UiElement Overrides                                                      */
/******************************************************************************/

static gboolean
ui_edge_contains_point (UiElement *element, gdouble x, gdouble y)
{
  (void) element;
  (void) x;
  (void) y;

  /* @Incomplete: Check if point is in sockets. Edge curve is handled by event window region.  */

  return TRUE;
}

static void
ui_edge_get_event_region (UiElement *element, cairo_region_t *region)
{
  UiEdge *edge;
  UiEdgePrivate *priv;
  GtkAllocation allocation;
  UiSocket source;
  UiSocket sink;
  gdouble x1, y1, x2, y2, x3, y3, x4, y4;
  gdouble min_radius;
  gdouble arc_length;
  guint num_steps;

  edge = UI_EDGE (element);
  priv = edge->priv;
  gtk_widget_get_allocation (GTK_WIDGET (element), &allocation);
  ui_node_get_sink_socket (priv->source, priv->sink, &source);
  ui_node_get_source_socket (priv->sink, priv->source, &sink);

  if (priv->source == priv->detached)
    {
      source.x = priv->detached_x;
      source.y = priv->detached_y;
      source.angle = M_PI * .5;
    }
  else if (priv->sink == priv->detached)
    {
      sink.x = priv->detached_x;
      sink.y = priv->detached_y;
      sink.angle = M_PI * 1.5;
    }

  x1 = (gdouble) source.x - allocation.x;
  y1 = (gdouble) source.y - allocation.y;
  x2 = x1 + (cos (source.angle) * SOCKET_EXTRUSION);
  y2 = y1 + (sin (source.angle) * SOCKET_EXTRUSION);
  x4 = (gdouble) sink.x - allocation.x;
  y4 = (gdouble) sink.y - allocation.y;
  x3 = x4 + (cos (sink.angle) * SOCKET_EXTRUSION);
  y3 = y4 + (sin (sink.angle) * SOCKET_EXTRUSION);
  min_radius = MIN (source.radius, sink.radius);

  /* Not real arc length, just a rough overshooting estimation by adding up the
     length of the control points path.  */
  arc_length = sqrt ((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1))
    + sqrt ((x3 - x2) * (x3 - x2) + (y3 - y2) * (y3 - y2))
    + sqrt ((x4 - x3) * (x4 - x3) + (y4 - y3) * (y4 - y3));
  num_steps = round (arc_length / min_radius);

  min_radius *= 1.5; /* @Arbitrary expansion of region that seems to work well for now.  */
  for (guint i = 0; i <= num_steps; ++i)
    {
      cairo_rectangle_int_t rect;
      gdouble t = (gdouble) i / num_steps;
      gdouble x = get_curve_point_1d (x1, x2, x3, x4, t);
      gdouble y = get_curve_point_1d (y1, y2, y3, y4, t);

      rect.x = (int) round (x) - min_radius / 2.;
      rect.y = (int) round (y) - min_radius / 2.;
      rect.width = min_radius;
      rect.height = min_radius;

      cairo_region_union_rectangle (region, &rect);
    }
}

static gboolean
on_anim_timeout (gpointer data)
{
  UiEdge *edge = UI_EDGE (data);
  UiEdgePrivate *priv = edge->priv;

  priv->anim_phase = fmod (priv->anim_phase + .025, 1.);
  gtk_widget_queue_draw (GTK_WIDGET (edge));

  return TRUE;
}

/******************************************************************************/
/* Helper functions                                                           */
/******************************************************************************/

static gdouble
get_curve_point_1d (gdouble x1, gdouble x2, gdouble x3, gdouble x4, gdouble t)
{
  return x1 * (1. - t) * (1. - t) * (1. - t)
    + 3. * x2 * t * (1. - t) * (1. - t)
    + 3. * x3 * t * t * (1. - t)
    + x4 * t * t * t;
}

static void
get_bounds_for_curve (gdouble x1, gdouble y1,
                      gdouble x2, gdouble y2,
                      gdouble x3, gdouble y3,
                      gdouble x4, gdouble y4,
                      GdkRectangle *box)
{
  gdouble xl = x1; /* Lowest X */
  gdouble xh = x1; /* Highest X */
  gdouble yl = y1; /* Lowest Y */
  gdouble yh = y1; /* Highest Y */
  gdouble a;
  gdouble b;
  gdouble c;
  gdouble disc;
  gdouble t, tx;

  /* Find X bounds.  */
  a = (3. * x4) - (9. * x3) + (9. * x2) - (3. * x1);
  b = (6. * x1) - (12. * x2) + (6. * x3);
  c = (3. * x2) - (3. * x1);
  disc = (b * b) - (4. * a * c);

  if (x4 < xl)
    xl = x4;
  if (x4 > xh)
    xh = x4;

  if (disc >= 0)
    {
      t = (-b + sqrt (disc)) / (2. * a);
      if (t > 0. && t < 1.)
        {
          tx = get_curve_point_1d (x1, x2, x3, x4, t);
          if (tx < xl)
            xl = tx;
          if (tx > xh)
            xh = tx;
        }

      t = (-b - sqrt (disc)) / (2. * a);
      if (t > 0. && t < 1.)
        {
          tx = get_curve_point_1d (x1, x2, x3, x4, t);
          if (tx < xl)
            xl = tx;
          if (tx > xh)
            xh = tx;
        }
    }

  /* Find Y bounds.  */
  a = (3. * y4) - (9. * y3) + (9. * y2) - (3. * y1);
  b = (6. * y1) - (12. * y2) + (6. * y3);
  c = (3. * y2) - (3. * y1);
  disc = (b * b) - (4 * a * c);

  if (y4 < yl)
    yl = y4;
  if (y4 > yh)
    yh = y4;

  if (disc >= 0)
    {
      t = (-b + sqrt (disc)) / (2. * a);
      if (t > 0. && t < 1.)
        {
          tx = get_curve_point_1d (y1, y2, y3, y4, t);
          if (tx < yl)
            yl = tx;
          if (tx > yh)
            yh = tx;
        }

      t = (-b - sqrt (disc)) / (2. * a);
      if (t > 0. && t < 1.)
        {
          tx = get_curve_point_1d (y1, y2, y3, y4, t);
          if (tx < yl)
            yl = tx;
          if (tx > yh)
            yh = tx;
        }
    }

  box->x = (int) round (xl);
  box->y = (int) round (yl);
  box->width = (int) round (xh - xl);
  box->height = (int) round (yh - yl);
}
