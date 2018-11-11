#ifndef GTK3_GRAPH_H
#define GTK3_GRAPH_H 1

#include <gtk/gtk.h>

#include "node.h"
#include "edge.h"

G_BEGIN_DECLS

#define UI_TYPE_GRAPH (ui_graph_get_type ())
#define UI_GRAPH(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), UI_TYPE_GRAPH, UiGraph))
#define UI_GRAPH_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), UI_TYPE_GRAPH, UiGraphClass))
#define UI_IS_GRAPH(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), UI_TYPE_GRAPH))
#define UI_IS_GRAPH_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), UI_TYPE_GRAF))
#define UI_GRAPH_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), UI_TYPE_GRAPH, UiGraphClass))

typedef struct _UiGraph UiGraph;
typedef struct _UiGraphPrivate UiGraphPrivate;
typedef struct _UiGraphClass UiGraphClass;

struct _UiGraph
{
  GtkContainer container;
  UiGraphPrivate *priv;
};

struct _UiGraphClass
{
  GtkContainerClass parent_class;
  void (*edge_opened) (UiGraph *graph, UiNode *source, UiNode *sink);
  void (*edge_closed) (UiGraph *graph, UiNode *source, UiNode *sink);
};

GType       ui_graph_get_type   (void) G_GNUC_CONST;
GtkWidget  *ui_graph_new        (void);
void        ui_graph_put        (UiGraph *graph,
                                 UiNode *node,
                                 gdouble x,
                                 gdouble y);
void        ui_graph_move       (UiGraph *graph,
                                 UiNode *node,
                                 gdouble x,
                                 gdouble y);
UiEdge     *ui_graph_connect    (UiGraph *graph,
                                 UiNode *source,
                                 UiNode *sink);

G_END_DECLS

#endif /* ! GTK3_GRAPH_H */
