#ifndef GTK3_ELEMENT_H
#define GTK3_ELEMENT_H 1

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define UI_TYPE_ELEMENT (ui_element_get_type ())
#define UI_ELEMENT(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), UI_TYPE_ELEMENT, UiElement))
#define UI_ELEMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), UI_TYPE_ELEMENT, UiElementClass))
#define UI_IS_ELEMENT(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), UI_TYPE_ELEMENT))
#define UI_IS_ELEMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), UI_TYPE_ELEMENT))
#define UI_ELEMENT_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), UI_TYPE_ELEMENT, UiElementClass))

typedef struct _UiElement UiElement;
typedef struct _UiElementPrivate UiElementPrivate;
typedef struct _UiElementClass UiElementClass;

struct _UiElement
{
  GtkBin bin;
  UiElementPrivate *priv;
};

struct _UiElementClass
{
  GtkBinClass parent_class;
  void      (*clicked)          (UiElement *element);
  void      (*hover_start)      (UiElement *element);
  void      (*hover_end)        (UiElement *element);
  gboolean  (*contains_point)   (UiElement *element, gdouble x, gdouble y);
  void      (*get_event_region) (UiElement *element, cairo_region_t *region);
};

GType       ui_element_get_type         (void) G_GNUC_CONST;
gboolean    ui_element_contains_point   (UiElement *element,
                                         gdouble x,
                                         gdouble y);
gboolean    ui_element_is_hovering      (UiElement *element);
gboolean    ui_element_is_pressed       (UiElement *element);
GdkWindow  *ui_element_get_event_window (UiElement *element);
GtkPopover *ui_element_get_popover      (UiElement *element);

G_END_DECLS

#endif /* ! GTK3_ELEMENT_H */
