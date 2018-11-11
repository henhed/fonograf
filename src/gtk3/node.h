#ifndef GTK3_NODE_H
#define GTK3_NODE_H 1

#include <gtk/gtk.h>

#include "element.h"

G_BEGIN_DECLS

#define UI_TYPE_NODE (ui_node_get_type ())
#define UI_NODE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), UI_TYPE_NODE, UiNode))
#define UI_NODE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), UI_TYPE_NODE, UiNodeClass))
#define UI_IS_NODE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), UI_TYPE_NODE))
#define UI_IS_NODE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), UI_TYPE_NODE))
#define UI_NODE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), UI_TYPE_NODE, UiNodeClass))

typedef struct _UiNode UiNode;
typedef struct _UiNodePrivate UiNodePrivate;
typedef struct _UiNodeClass UiNodeClass;
typedef struct _UiNodeStyle UiNodeStyle;
typedef struct _UiSocket UiSocket;

struct _UiNode
{
  UiElement element;
  UiNodePrivate *priv;
};

struct _UiNodeClass
{
  UiElementClass parent_class;
  void         (*rim_pressed) (UiNode *node, gboolean source);
};

struct _UiNodeStyle
{
  GdkRGBA bg;
  gdouble source_radius;
  gdouble sink_radius;
  gdouble source_margin;
  gdouble sink_margin;
  gdouble outline_width;
};

struct _UiSocket
{
  gdouble x;
  gdouble y;
  gdouble radius;
  gdouble angle;
};

GType               ui_node_get_type            (void) G_GNUC_CONST;
GtkWidget          *ui_node_new                 (void);
GtkWidget          *ui_node_new_with_label      (const gchar *label);
gboolean            ui_node_add_source          (UiNode *node,
                                                 UiNode *source);
gboolean            ui_node_add_sink            (UiNode *node,
                                                 UiNode *sink);
gboolean            ui_node_remove_source       (UiNode *node,
                                                 UiNode *source);
gboolean            ui_node_remove_sink         (UiNode *node,
                                                 UiNode *sink);
gboolean            ui_node_get_source_socket   (const UiNode *node,
                                                 const UiNode *source,
                                                 UiSocket *socket);
gboolean            ui_node_get_sink_socket     (const UiNode *node,
                                                 const UiNode *sink,
                                                 UiSocket *socket);
const UiNodeStyle  *ui_node_get_style           (const UiNode *node);

G_END_DECLS

#endif /* ! GTK3_NODE_H */
