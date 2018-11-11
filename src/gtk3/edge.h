#ifndef GTK3_EDGE_H
#define GTK3_EDGE_H 1

#include <gtk/gtk.h>

#include "element.h"
#include "node.h"

G_BEGIN_DECLS

#define UI_TYPE_EDGE (ui_edge_get_type ())
#define UI_EDGE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), UI_TYPE_EDGE, UiEdge))
#define UI_EDGE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), UI_TYPE_EDGE, UiEdgeClass))
#define UI_IS_EDGE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), UI_TYPE_EDGE))
#define UI_IS_EDGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), UI_TYPE_EDGE))
#define UI_EDGE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), UI_TYPE_EDGE, UiEdgeClass))

typedef struct _UiEdge UiEdge;
typedef struct _UiEdgePrivate UiEdgePrivate;
typedef struct _UiEdgeClass UiEdgeClass;

struct _UiEdge
{
  UiElement element;
  UiEdgePrivate *priv;
};

struct _UiEdgeClass
{
  UiElementClass parent_class;
  void         (*attached)              (UiEdge *edge, UiNode *attached,
                                         UiNode *detached);
  void         (*detached)              (UiEdge *edge, UiNode *detached);
};

GType       ui_edge_get_type            (void) G_GNUC_CONST;
GtkWidget  *ui_edge_new                 (UiNode *source, UiNode *sink);
void        ui_edge_get_bounds          (UiEdge *edge, GdkRectangle *bounds);
UiNode     *ui_edge_get_source          (UiEdge *edge);
UiNode     *ui_edge_get_sink            (UiEdge *edge);
void        ui_edge_set_weight          (UiEdge *edge, gdouble weight);
gboolean    ui_edge_detach              (UiEdge *edge, UiNode *node);
UiNode     *ui_edge_get_detached        (UiEdge *edge);
void        ui_edge_set_detached_pos    (UiEdge *edge, gdouble x, gdouble y);
gboolean    ui_edge_attach              (UiEdge *edge, UiNode *node);

G_END_DECLS

#endif /* ! GTK3_EDGE_H */
