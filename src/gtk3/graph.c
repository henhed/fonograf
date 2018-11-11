#include <math.h>

#include "graph.h"

#ifndef GTK_PARAM_READWRITE
# define GTK_PARAM_READWRITE                    \
  G_PARAM_READWRITE |                           \
  G_PARAM_STATIC_NAME |                         \
  G_PARAM_STATIC_NICK |                         \
  G_PARAM_STATIC_BLURB
#endif

struct _UiGraphPrivate
{
  GList *nodes;
  GList *edges;
  UiNode *ground_node;
  UiEdge *ground_edge;
};

typedef struct
{
  UiGraph *graph;
  UiNode *node;
  gdouble x;
  gdouble y;
  gboolean dragging;
  gdouble drag_start_x;
  gdouble drag_start_y;
  gboolean new_source_drag;
  gboolean new_sink_drag;
} UiGraphNode;

typedef struct
{
  UiGraph *graph;
  UiEdge *edge;
  gboolean dragging;
  gdouble drag_start_x;
  gdouble drag_start_y;
  gdouble detached;
} UiGraphEdge;

enum {
  NODE_PROP_0,
  NODE_PROP_X,
  NODE_PROP_Y
};

enum {
  EDGE_OPENED,
  EDGE_CLOSED,
  NUM_SIGNALS
};

/* Widget class overrides.  */
static void ui_graph_size_allocate (GtkWidget *, GtkAllocation *);
static gboolean ui_graph_draw (GtkWidget *, cairo_t *);

/* Container class overrides.  */
static void ui_graph_add (GtkContainer *, GtkWidget *);
static void ui_graph_remove (GtkContainer *, GtkWidget *);
static void ui_graph_forall (GtkContainer *, gboolean, GtkCallback, gpointer);
static GType ui_graph_child_type (GtkContainer *);
static void ui_graph_set_child_property (GtkContainer *, GtkWidget *, guint,
                                         const GValue *, GParamSpec *);
static void ui_graph_get_child_property (GtkContainer *, GtkWidget *, guint,
                                         GValue *, GParamSpec *);

/* Signal handlers.  */
static gboolean on_node_button_press (GtkWidget *, GdkEventButton *, UiGraphNode *);
static gboolean on_node_button_release (GtkWidget *, GdkEventButton *, UiGraphNode *);
static gboolean on_node_motion_notify (GtkWidget *, GdkEventMotion *, UiGraphNode *);
static void on_node_rim_pressed (UiNode *, gboolean, UiGraphNode *);
static gboolean on_edge_button_press (GtkWidget *, GdkEventButton *, UiGraphEdge *);
static gboolean on_edge_button_release (GtkWidget *, GdkEventButton *, UiGraphEdge *);
static gboolean on_edge_motion_notify (GtkWidget *, GdkEventMotion *, UiGraphEdge *);

static guint graph_signals[NUM_SIGNALS] = {0};

G_DEFINE_TYPE_WITH_PRIVATE (UiGraph, ui_graph, GTK_TYPE_CONTAINER)

static void
ui_graph_class_init (UiGraphClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);

  widget_class->size_allocate = ui_graph_size_allocate;
  widget_class->draw = ui_graph_draw;
  gtk_widget_class_set_css_name (widget_class, "fonograf-graph");

  container_class->add = ui_graph_add;
  container_class->remove = ui_graph_remove;
  container_class->forall = ui_graph_forall;
  container_class->child_type = ui_graph_child_type;
  container_class->set_child_property = ui_graph_set_child_property;
  container_class->get_child_property = ui_graph_get_child_property;
  gtk_container_class_handle_border_width (container_class);

  gtk_container_class_install_child_property (container_class,
                                              NODE_PROP_X,
                                              g_param_spec_double ("x",
                                                                   "X position",
                                                                   "X position of node",
                                                                   0., 1., .5,
                                                                   GTK_PARAM_READWRITE));

  gtk_container_class_install_child_property (container_class,
                                              NODE_PROP_Y,
                                              g_param_spec_double ("y",
                                                                   "Y position",
                                                                   "Y position of node",
                                                                   0., 1., .5,
                                                                   GTK_PARAM_READWRITE));

  graph_signals[EDGE_OPENED] = g_signal_new ("edge_opened",
                                             G_OBJECT_CLASS_TYPE (class),
                                             G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                             G_STRUCT_OFFSET (UiGraphClass, edge_opened),
                                             NULL, NULL,
                                             NULL,
                                             G_TYPE_NONE,
                                             2, UI_TYPE_NODE, UI_TYPE_NODE);

  graph_signals[EDGE_CLOSED] = g_signal_new ("edge_closed",
                                             G_OBJECT_CLASS_TYPE (class),
                                             G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                             G_STRUCT_OFFSET (UiGraphClass, edge_closed),
                                             NULL, NULL,
                                             NULL,
                                             G_TYPE_NONE,
                                             2, UI_TYPE_NODE, UI_TYPE_NODE);
}

static void
ui_graph_init (UiGraph *graph)
{
  UiGraphPrivate *priv;

  graph->priv = ui_graph_get_instance_private (graph);
  priv = graph->priv;

  priv->nodes = NULL;
  priv->edges = NULL;
  priv->ground_node = UI_NODE (ui_node_new ()); /* @Leak: Never freed.  */
  priv->ground_edge = NULL;

  /* @Cleanup: Necessary for edge style.  */
  gtk_widget_set_parent (GTK_WIDGET (priv->ground_node), GTK_WIDGET (graph));
  gtk_widget_show (GTK_WIDGET (priv->ground_node));

  gtk_widget_set_has_window (GTK_WIDGET (graph), FALSE);
}

static UiGraphNode *
get_node_info (UiGraph *graph, UiNode *node)
{
  UiGraphPrivate *priv = graph->priv;
  GList *nodes;

  for (nodes = priv->nodes; nodes; nodes = nodes->next)
    {
      UiGraphNode *node_info = nodes->data;

      if (node_info->node == node)
        return node_info;
    }

  return NULL;
}

static void
ui_graph_move_internal (UiGraph *graph, UiGraphNode *node_info,
                        gdouble x, gdouble y)
{
  GtkWidget *node_widget = GTK_WIDGET (node_info->node);

  g_return_if_fail (UI_IS_GRAPH (graph));
  g_return_if_fail (gtk_widget_get_parent (node_widget) == GTK_WIDGET (graph));

  gtk_widget_freeze_child_notify (node_widget);

  if (node_info->x != x)
    {
      node_info->x = x;
      gtk_widget_child_notify (node_widget, "x");
    }

  if (node_info->y != y)
    {
      node_info->y = y;
      gtk_widget_child_notify (node_widget, "y");
    }

  gtk_widget_thaw_child_notify (node_widget);

  if (gtk_widget_get_visible (node_widget)
      && gtk_widget_get_visible (GTK_WIDGET (graph)))
    {
      GList *edges;

      /* Queue allocation for connected nodes to update socket ordering.  */
      for (edges = graph->priv->edges; edges; edges = edges->next)
        {
          UiGraphEdge *edge_info = edges->data;
          UiEdge *edge = edge_info->edge;
          if (ui_edge_get_source (edge) == node_info->node)
            gtk_widget_queue_allocate (GTK_WIDGET (ui_edge_get_sink (edge)));
          else if (ui_edge_get_sink (edge) == node_info->node)
            gtk_widget_queue_allocate (GTK_WIDGET (ui_edge_get_source (edge)));
        }

      gtk_widget_queue_allocate (GTK_WIDGET (graph));
    }
}

/******************************************************************************/
/* API                                                                        */
/******************************************************************************/

GtkWidget *
ui_graph_new ()
{
  return g_object_new (UI_TYPE_GRAPH, NULL);
}

void
ui_graph_put (UiGraph *graph, UiNode *node, gdouble x, gdouble y)
{
  UiGraphPrivate *priv;
  UiGraphNode *node_info;

  g_return_if_fail (UI_IS_GRAPH (graph));
  g_return_if_fail (UI_IS_NODE (node));
  g_return_if_fail (gtk_widget_get_parent (GTK_WIDGET (node)) == NULL);

  priv = graph->priv;

  node_info = g_new (UiGraphNode, 1);
  node_info->graph = graph;
  node_info->node = node;
  node_info->x = x;
  node_info->y = y;
  node_info->dragging = FALSE;
  node_info->drag_start_x = 0.;
  node_info->drag_start_y = 0.;
  node_info->new_source_drag = FALSE;
  node_info->new_sink_drag = FALSE;

  gtk_widget_set_parent (GTK_WIDGET (node), GTK_WIDGET (graph));

  priv->nodes = g_list_append (priv->nodes, node_info);

  g_signal_connect (GTK_WIDGET (node), "button-press-event",
                    G_CALLBACK (on_node_button_press), node_info);
  g_signal_connect (GTK_WIDGET (node), "button-release-event",
                    G_CALLBACK (on_node_button_release), node_info);
  g_signal_connect (GTK_WIDGET (node), "motion-notify-event",
                    G_CALLBACK (on_node_motion_notify), node_info);
  g_signal_connect (GTK_WIDGET (node), "rim-pressed",
                    G_CALLBACK (on_node_rim_pressed), node_info);
}

void
ui_graph_move (UiGraph *graph, UiNode *node, gdouble x, gdouble y)
{
  ui_graph_move_internal (graph, get_node_info (graph, node), x, y);
}

UiEdge *
ui_graph_connect (UiGraph *graph, UiNode *source, UiNode *sink)
{
  UiGraphPrivate *priv;
  GtkWidget *edge;
  UiGraphEdge *edge_info;

  g_return_val_if_fail (UI_IS_GRAPH (graph), NULL);
  g_return_val_if_fail (UI_IS_NODE (source), NULL);
  g_return_val_if_fail (UI_IS_NODE (sink), NULL);
  g_return_val_if_fail (source != sink, NULL);
  g_return_val_if_fail (gtk_widget_get_parent (GTK_WIDGET (source)) == GTK_WIDGET (graph), NULL);
  g_return_val_if_fail (gtk_widget_get_parent (GTK_WIDGET (sink)) == GTK_WIDGET (graph), NULL);

  priv = graph->priv;

  /* Allocate sockets in connecting nodes.  */
  ui_node_add_sink (source, sink);  /* @Incomplete: Respect return value.  */
  ui_node_add_source (sink, source);  /* @Incomplete: Respect return value.  */

  /* Create edge widget.  */
  edge = ui_edge_new (source, sink);
  gtk_widget_set_parent (edge, GTK_WIDGET (graph));
  gtk_widget_show (edge);

  edge_info = g_new (UiGraphEdge, 1);
  *edge_info = (UiGraphEdge) {
    .graph = graph,
    .edge = UI_EDGE (edge),
    .dragging = FALSE,
    .drag_start_x = 0.,
    .drag_start_y = 0.,
    .detached = FALSE
  };
  priv->edges = g_list_append (priv->edges, edge_info);

  g_signal_connect (GTK_WIDGET (edge), "button-press-event",
                    G_CALLBACK (on_edge_button_press), edge_info);
  g_signal_connect (GTK_WIDGET (edge), "button-release-event",
                    G_CALLBACK (on_edge_button_release), edge_info);
  g_signal_connect (GTK_WIDGET (edge), "motion-notify-event",
                    G_CALLBACK (on_edge_motion_notify), edge_info);

  return UI_EDGE (edge);
}

/******************************************************************************/
/* GtkWidget Overrides                                                        */
/******************************************************************************/

static void
ui_graph_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
  UiGraph *graph = UI_GRAPH (widget);
  UiGraphPrivate *priv = graph->priv;
  UiGraphNode *node_info;
  GtkAllocation node_allocation;
  GtkRequisition node_requisition;
  GList *nodes;
  GList *edges;
  double gw = (double) allocation->width;
  double gh = (double) allocation->height;

  gtk_widget_set_allocation (widget, allocation);

  if (gtk_widget_get_has_window (widget))
    {
      if (gtk_widget_get_realized (widget))
        gdk_window_move_resize (gtk_widget_get_window (widget),
                                allocation->x,
                                allocation->y,
                                allocation->width,
                                allocation->height);
    }

  /* @Cleanup: Necessary for edge style.  */
  gtk_widget_get_preferred_size (GTK_WIDGET (priv->ground_node),
                                 &node_requisition, NULL);
  node_allocation.x = 0;
  node_allocation.y = 0;
  node_allocation.width = node_requisition.width;
  node_allocation.height = node_requisition.height;
  gtk_widget_size_allocate (GTK_WIDGET (priv->ground_node), &node_allocation);

  for (nodes = priv->nodes; nodes; nodes = nodes->next)
    {
      GtkWidget *node_widget;
      double nx, ny, nw, nh;

      node_info = nodes->data;
      node_widget = GTK_WIDGET (node_info->node);

      if (!gtk_widget_get_visible (node_widget))
        continue;

      gtk_widget_get_preferred_size (node_widget, &node_requisition, NULL);
      nw = (double) node_requisition.width;
      nh = (double) node_requisition.height;
      nx = (node_info->x * gw) - (nw / 2.);
      ny = (node_info->y * gh) - (nh / 2.);

      node_allocation.x = (gint) nx;
      node_allocation.y = (gint) ny;

      if (!gtk_widget_get_has_window (widget))
        {
          node_allocation.x += allocation->x;
          node_allocation.y += allocation->y;
        }

      node_allocation.width = node_requisition.width;
      node_allocation.height = node_requisition.height;
      gtk_widget_size_allocate (node_widget, &node_allocation);
    }

  for (edges = priv->edges; edges; edges = edges->next)
    {
      UiGraphEdge *edge_info = edges->data;
      UiEdge *edge = edge_info->edge;
      GdkRectangle edge_bounds;
      GtkAllocation edge_allocation;

      /* Let edge calculate its own allocation based on node postion.  */
      ui_edge_get_bounds (edge, &edge_bounds);

      edge_allocation.x = edge_bounds.x;
      edge_allocation.y = edge_bounds.y;
      edge_allocation.width = edge_bounds.width;
      edge_allocation.height = edge_bounds.height;

      gtk_widget_size_allocate (GTK_WIDGET (edge), &edge_allocation);
    }
}

static gboolean
ui_graph_draw (GtkWidget *widget, cairo_t *cr)
{
  UiGraph *graph = UI_GRAPH (widget);
  UiGraphPrivate *priv = graph->priv;
  GtkStyleContext *context;
  GtkAllocation allocation;
  UiGraphNode *node_info;
  UiGraphEdge *edge_info;
  GList *nodes;
  GList *edges;

  /* Draw graph surface.  */
  context = gtk_widget_get_style_context (widget);
  gtk_widget_get_allocation (widget, &allocation);
  gtk_render_background (context, cr, 0, 0,
                         (gdouble) allocation.width,
                         (gdouble) allocation.height);

  /* Draw edges.  */
  for (edges = priv->edges; edges; edges = edges->next)
    {
      edge_info = edges->data;
      if (edge_info->edge == priv->ground_edge)
        continue; /* Draw last if non-null.  */
      gtk_container_propagate_draw (GTK_CONTAINER (graph),
                                    GTK_WIDGET (edge_info->edge),
                                    cr);
    }

  /* Draw nodes.  */
  for (nodes = priv->nodes; nodes; nodes = nodes->next)
    {
      node_info = nodes->data;
      gtk_container_propagate_draw (GTK_CONTAINER (graph),
                                    GTK_WIDGET (node_info->node),
                                    cr);
    }

  if (priv->ground_edge != NULL)
    gtk_container_propagate_draw (GTK_CONTAINER (graph),
                                  GTK_WIDGET (priv->ground_edge),
                                  cr);

  return FALSE;
}

/******************************************************************************/
/* GtkContainer Overrides                                                     */
/******************************************************************************/

static void
ui_graph_add (GtkContainer *container, GtkWidget *widget)
{
  ui_graph_put (UI_GRAPH (container), UI_NODE (widget), .5, .5);
}

static void
ui_graph_remove (GtkContainer *container, GtkWidget *widget)
{
  UiGraph *graph = UI_GRAPH (container);
  UiGraphPrivate *priv = graph->priv;
  UiGraphNode *node_info;
  UiGraphEdge *edge_info;
  GList *nodes;
  GList *edges;

  for (nodes = priv->nodes; nodes; nodes = nodes->next)
    {
      node_info = nodes->data;

      if (GTK_WIDGET (node_info->node) == widget)
        {
          gtk_widget_unparent (widget);

          priv->nodes = g_list_remove_link (priv->nodes, nodes);
          g_list_free (nodes);
          g_free (node_info);

          /* @Leak: Remove edges associated with node.  */

          break;
        }
    }

  for (edges = priv->edges; edges; edges = edges->next)
    {
      edge_info = edges->data;

      if (GTK_WIDGET (edge_info->edge) == widget)
        {
          gtk_widget_unparent (widget);

          priv->edges = g_list_remove_link (priv->edges, edges);
          g_list_free (edges);
          g_free (edge_info);

          break;
        }
    }
}

static void
ui_graph_forall (GtkContainer *container, gboolean include_internals,
                 GtkCallback callback, gpointer callback_data)
{
  (void) include_internals;

  UiGraph *graph = UI_GRAPH (container);
  UiGraphPrivate *priv = graph->priv;
  UiGraphNode *node_info;
  UiGraphEdge *edge_info;
  GList *nodes;
  GList *edges;

  nodes = priv->nodes;
  while (nodes)
    {
      node_info = nodes->data;
      nodes = nodes->next;

      (*callback) (GTK_WIDGET (node_info->node), callback_data);
    }

  if (include_internals)
    {
      edges = priv->edges;
      while (edges)
        {
          edge_info = edges->data;
          edges = edges->next;

          (*callback) (GTK_WIDGET (edge_info->edge), callback_data);
        }
    }
}

static GType
ui_graph_child_type (GtkContainer *container)
{
  (void) container;

  return UI_TYPE_NODE;
}

static void
ui_graph_set_child_property (GtkContainer *container, GtkWidget *child,
                             guint property_id, const GValue *value,
                             GParamSpec *pspec)
{
  UiGraph *graph = UI_GRAPH (container);
  UiGraphNode *node_info;

  node_info = get_node_info (graph, UI_NODE (child));

  switch (property_id)
    {
    case NODE_PROP_X:
      ui_graph_move_internal (graph,
                              node_info,
                              g_value_get_double (value),
                              node_info->y);
      break;
    case NODE_PROP_Y:
      ui_graph_move_internal (graph,
                              node_info,
                              node_info->x,
                              g_value_get_double (value));
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
ui_graph_get_child_property (GtkContainer *container, GtkWidget *child,
                             guint property_id, GValue *value,
                             GParamSpec *pspec)
{
  UiGraphNode *node_info;

  node_info = get_node_info (UI_GRAPH (container), UI_NODE (child));

  switch (property_id)
    {
    case NODE_PROP_X:
      g_value_set_double (value, node_info->x);
      break;
    case NODE_PROP_Y:
      g_value_set_double (value, node_info->y);
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

/******************************************************************************/
/* Signal Handlers                                                            */
/******************************************************************************/

static UiNode *
find_closest_open_socket_node (UiGraph *graph, gdouble x, gdouble y)
{
  UiNode *closest = NULL;
  gdouble threshold = 32; /* @Arbitrary */
  GList *nodes;

  for (nodes = graph->priv->nodes; nodes; nodes = nodes->next)
    {
      UiGraphNode *node_info;
      UiSocket socket;

      node_info = nodes->data;
      if (ui_node_get_source_socket (node_info->node, NULL, &socket)
          || ui_node_get_sink_socket (node_info->node, NULL, &socket))
        {
          gdouble dx = socket.x - x;
          gdouble dy = socket.y - y;
          gdouble dist = sqrt ((dx * dx) + (dy * dy));
          if (dist < threshold)
            {
              threshold = dist;
              closest = node_info->node;
            }
        }
    }

  return closest;
}

static gboolean
on_node_button_press (GtkWidget *widget, GdkEventButton *event,
                      UiGraphNode *pressed)
{
  UiGraph *graph = pressed->graph;
  UiGraphPrivate *priv = graph->priv;
  GtkAllocation allocation;
  GList *pressed_item = g_list_find (priv->nodes, pressed);

  gtk_widget_get_allocation (widget, &allocation);

  if (pressed->new_source_drag || pressed->new_sink_drag)
    {
      /* Drag on rim to create new edge.  */
      if (pressed->new_source_drag)
        {
          priv->ground_edge = ui_graph_connect (graph,
                                                priv->ground_node,
                                                pressed->node);
          g_signal_emit (graph, graph_signals[EDGE_OPENED], 0,
                         NULL, pressed->node);
        }
      else
        {
          priv->ground_edge = ui_graph_connect (graph,
                                                pressed->node,
                                                priv->ground_node);
          g_signal_emit (graph, graph_signals[EDGE_OPENED], 0,
                         pressed->node, NULL);
        }

      ui_edge_detach (priv->ground_edge, priv->ground_node);
      ui_edge_set_detached_pos (priv->ground_edge,
                                (gdouble) allocation.x + event->x,
                                (gdouble) allocation.y + event->y);

      gtk_widget_queue_allocate (widget);
      gtk_widget_queue_allocate (GTK_WIDGET (graph));
    }
  else
    {
      /* Regular node movement drag.  */
      pressed->dragging = TRUE;
      pressed->drag_start_x = event->x - (gdouble) allocation.width / 2;
      pressed->drag_start_y = event->y - (gdouble) allocation.height / 2;
    }

  if (pressed_item != NULL)
    {
      /* Put pressed node last to draw it on top.  */
      priv->nodes = g_list_remove_link (priv->nodes, pressed_item);
      priv->nodes = g_list_concat (priv->nodes, pressed_item);
    }

  return FALSE;
}

static void
on_node_rim_pressed (UiNode *node, gboolean source, UiGraphNode *pressed)
{
  (void) node;
  (void) pressed;

  /* Tell subsequent button press handler that the given node should not be
     dragged, but rather a new connection is to be made.  */
  if (source)
    {
      pressed->new_source_drag = TRUE;
      pressed->new_sink_drag = FALSE;
    }
  else
    {
      pressed->new_sink_drag = TRUE;
      pressed->new_source_drag = FALSE;
    }
}

static gboolean
on_node_button_release (GtkWidget *widget, GdkEventButton *event,
                        UiGraphNode *released)
{
  (void) widget;
  (void) event;

  UiGraph *graph = released->graph;
  UiGraphPrivate *priv = released->graph->priv;
  UiNode *attachee;
  GtkAllocation allocation;

  if (released->dragging)
    {
      released->dragging = FALSE;
      return FALSE;
    }

  if (priv->ground_edge != NULL)
    {
      gtk_container_remove (GTK_CONTAINER (graph),
                            GTK_WIDGET (priv->ground_edge));
      priv->ground_edge = NULL;
    }

  gtk_widget_get_allocation (widget, &allocation);
  attachee = find_closest_open_socket_node (graph,
                                            (gdouble) allocation.x + event->x,
                                            (gdouble) allocation.y + event->y);
  if (attachee == NULL)
    attachee = released->node;

  if (released->new_source_drag)
    {
      g_signal_emit (graph, graph_signals[EDGE_CLOSED], 0,
                     attachee, released->node);
    }

  if (released->new_sink_drag)
    {
      g_signal_emit (graph, graph_signals[EDGE_CLOSED], 0,
                     released->node, attachee);
    }

  released->new_source_drag = FALSE;
  released->new_sink_drag = FALSE;

  gtk_widget_queue_allocate (widget);
  gtk_widget_queue_allocate (GTK_WIDGET (graph));

  return FALSE;
}

static gboolean
on_node_motion_notify (GtkWidget *widget, GdkEventMotion *event,
                       UiGraphNode *moved)
{
  GtkAllocation allocation;
  gdouble x;
  gdouble y;

  if (moved->new_source_drag || moved->new_sink_drag)
    {
      gtk_widget_get_allocation (widget, &allocation);
      x = (gdouble) allocation.x + event->x;
      y = (gdouble) allocation.y + event->y;
      ui_edge_set_detached_pos (moved->graph->priv->ground_edge, x, y);
      gtk_widget_queue_allocate (GTK_WIDGET (moved->graph));
      return FALSE;
    }

  if (!moved->dragging)
    return FALSE;

  /* Calculate new pixel value coordinates.  */
  gtk_widget_get_allocation (widget, &allocation);
  x = (gdouble) allocation.x + event->x - moved->drag_start_x;
  y = (gdouble) allocation.y + event->y - moved->drag_start_y;

  /* Convert coordinates to fraction of graph size.  */
  gtk_widget_get_allocation (GTK_WIDGET (moved->graph), &allocation);
  x /= (gdouble) allocation.width;
  y /= (gdouble) allocation.height;

  x = MAX (0., MIN (1., x));
  y = MAX (0., MIN (1., y));

  ui_graph_move_internal (moved->graph, moved, x, y);

  return FALSE;
}

static gboolean
on_edge_button_press (GtkWidget *widget, GdkEventButton *event,
                      UiGraphEdge *pressed)
{
  (void) widget;

  pressed->dragging = TRUE;
  pressed->drag_start_x = event->x;
  pressed->drag_start_y = event->y;

  return FALSE;
}

static gboolean
on_edge_button_release (GtkWidget *widget, GdkEventButton *event,
                        UiGraphEdge *released)
{
  UiGraph *graph;
  UiNode *attachee;
  GtkAllocation allocation;

  released->dragging = FALSE;

  if (!released->detached)
    return FALSE;

  graph = released->graph;
  released->detached = FALSE;

  gtk_widget_get_allocation (widget, &allocation);
  attachee = find_closest_open_socket_node (graph,
                                            (gdouble) allocation.x + event->x,
                                            (gdouble) allocation.y + event->y);
  /* `released' may be destroyed by attachment signal, so it's important we
     don't refer to it after this point.  */
  ui_edge_attach (UI_EDGE (widget), attachee);
  gtk_widget_queue_allocate (GTK_WIDGET (graph));

  return FALSE;
}

static gboolean
on_edge_motion_notify (GtkWidget *widget, GdkEventMotion *event,
                       UiGraphEdge *moved)
{
  if (!moved->dragging)
    return FALSE;

  if (!moved->detached)
    {
      gdouble dx = event->x - moved->drag_start_x;
      gdouble dy = event->y - moved->drag_start_y;
      if (sqrt (dx * dx + dy * dy) > 32.) /* @Arbitrary: 32px snap.  */
        {
          GtkAllocation allocation;
          gdouble x, y;
          UiSocket source_socket;
          UiSocket sink_socket;
          UiNode *source;
          UiNode *sink;
          gdouble source_dx, source_dy;
          gdouble sink_dx, sink_dy;

          gtk_widget_get_allocation (widget, &allocation);
          x = (gdouble) allocation.x + moved->drag_start_x;
          y = (gdouble) allocation.y + moved->drag_start_y;
          source = ui_edge_get_source (moved->edge);
          sink = ui_edge_get_sink (moved->edge);
          ui_node_get_sink_socket (source, sink, &source_socket);
          ui_node_get_source_socket (sink, source, &sink_socket);
          source_dx = x - source_socket.x;
          source_dy = y - source_socket.y;
          sink_dx = x - sink_socket.x;
          sink_dy = y - sink_socket.y;

          moved->detached = TRUE;

          /* Compare distance from mouse pointer to decide whether to detach
             source or sink.  */
          if ((source_dx * source_dx + source_dy * source_dy)
              < (sink_dx * sink_dx + sink_dy * sink_dy))
            ui_edge_detach (moved->edge, source);
          else
            ui_edge_detach (moved->edge, sink);
        }
    }

  if (moved->detached)
    {
      gdouble x, y;
      GtkAllocation allocation;
      gtk_widget_get_allocation (widget, &allocation);
      x = event->x + (gdouble) allocation.x;
      y = event->y + (gdouble) allocation.y;
      ui_edge_set_detached_pos (UI_EDGE (widget), x, y);
      gtk_widget_queue_allocate (GTK_WIDGET (moved->graph));
    }

  return FALSE;
}
