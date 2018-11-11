#include <math.h>

#include "element.h"

struct _UiElementPrivate
{
  GdkWindow *event_window;
  cairo_region_t *event_region;
  gboolean is_pressed;
  gdouble press_x;
  gdouble press_y;
  gboolean is_hovering;
  GtkPopover *popover;
};

enum
{
  CLICKED,
  HOVER_START,
  HOVER_END,
  NUM_SIGNALS
};

/* Widget class overrides.  */
static void ui_element_realize (GtkWidget *);
static void ui_element_unrealize (GtkWidget *);
static void ui_element_map (GtkWidget *);
static void ui_element_unmap (GtkWidget *);
static void ui_element_size_allocate (GtkWidget *, GtkAllocation *);
static gboolean ui_element_draw (GtkWidget *, cairo_t *);

/* Hepler functions.  */
static void ui_element_update_event_region (UiElement *);

/* Signal handlers.  */
static gboolean on_button_press (GtkWidget *, GdkEventButton *, gpointer);
static gboolean on_button_release (GtkWidget *, GdkEventButton *, gpointer);
static gboolean on_motion_notify (GtkWidget *, GdkEventMotion *, gpointer);
static gboolean on_leave_notify (GtkWidget *, GdkEventCrossing *, gpointer);
static void on_clicked (UiElement *);

static guint element_signals[NUM_SIGNALS] = {0};

G_DEFINE_TYPE_WITH_PRIVATE (UiElement, ui_element, GTK_TYPE_BIN)

static void
ui_element_class_init (UiElementClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  widget_class->realize = ui_element_realize;
  widget_class->unrealize = ui_element_unrealize;
  widget_class->map = ui_element_map;
  widget_class->unmap = ui_element_unmap;
  widget_class->size_allocate = ui_element_size_allocate;
  widget_class->draw = ui_element_draw;

  class->clicked = on_clicked;
  class->contains_point = NULL;
  class->get_event_region = NULL;

  element_signals[CLICKED] = g_signal_new ("clicked",
                                           G_OBJECT_CLASS_TYPE (class),
                                           G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                           G_STRUCT_OFFSET (UiElementClass, clicked),
                                           NULL, NULL,
                                           NULL,
                                           G_TYPE_NONE, 0);

  element_signals[HOVER_START] = g_signal_new ("hover-start",
                                               G_OBJECT_CLASS_TYPE (class),
                                               G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                               G_STRUCT_OFFSET (UiElementClass, hover_start),
                                               NULL, NULL,
                                               NULL,
                                               G_TYPE_NONE, 0);

  element_signals[HOVER_END] = g_signal_new ("hover-end",
                                             G_OBJECT_CLASS_TYPE (class),
                                             G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                             G_STRUCT_OFFSET (UiElementClass, hover_end),
                                             NULL, NULL,
                                             NULL,
                                             G_TYPE_NONE, 0);
}

static void
ui_element_init (UiElement *element)
{
  UiElementPrivate *priv;

  element->priv = ui_element_get_instance_private (element);
  priv = element->priv;
  priv->event_window = NULL;
  priv->event_region = NULL;
  priv->is_pressed = FALSE;
  priv->press_x = 0.;
  priv->press_y = 0.;
  priv->is_hovering = FALSE;
  priv->popover = GTK_POPOVER (gtk_popover_new (GTK_WIDGET (element))); /* @Leak: Never freed.  */
  gtk_popover_set_modal (priv->popover, FALSE);

  gtk_widget_set_has_window (GTK_WIDGET (element), FALSE);
  gtk_widget_set_can_focus (GTK_WIDGET (element), TRUE);

  g_signal_connect (GTK_WIDGET (element), "button-press-event",
                    G_CALLBACK (on_button_press), NULL);
  g_signal_connect (GTK_WIDGET (element), "button-release-event",
                    G_CALLBACK (on_button_release), NULL);
  g_signal_connect (GTK_WIDGET (element), "motion-notify-event",
                    G_CALLBACK (on_motion_notify), NULL);
  g_signal_connect (GTK_WIDGET (element), "leave-notify-event",
                    G_CALLBACK (on_leave_notify), NULL);
}

/******************************************************************************/
/* API                                                                        */
/******************************************************************************/

gboolean
ui_element_contains_point (UiElement *element, gdouble x, gdouble y)
{
  UiElementClass *class;

  g_return_val_if_fail (UI_IS_ELEMENT (element), FALSE);

  class = UI_ELEMENT_GET_CLASS (element);
  if (class->contains_point != NULL)
    return class->contains_point (element, x, y);

  return TRUE;
}

gboolean
ui_element_is_hovering (UiElement *element)
{
  g_return_val_if_fail (UI_IS_ELEMENT (element), FALSE);
  return element->priv->is_hovering;
}

gboolean
ui_element_is_pressed (UiElement *element)
{
  g_return_val_if_fail (UI_IS_ELEMENT (element), FALSE);
  return element->priv->is_pressed;
}

GdkWindow *
ui_element_get_event_window (UiElement *element)
{
  g_return_val_if_fail (UI_IS_ELEMENT (element), NULL);
  return element->priv->event_window;
}

GtkPopover *
ui_element_get_popover (UiElement *element)
{
  g_return_val_if_fail (UI_IS_ELEMENT (element), NULL);
  return element->priv->popover;
}

/******************************************************************************/
/* GtkWidget Overrides                                                        */
/******************************************************************************/

static void
ui_element_realize (GtkWidget *widget)
{
  UiElement *element = UI_ELEMENT (widget);
  UiElementPrivate *priv = element->priv;
  GtkAllocation allocation;
  GdkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;

  gtk_widget_get_allocation (widget, &allocation);

  gtk_widget_set_realized (widget, TRUE);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_BUTTON_PRESS_MASK
                            | GDK_BUTTON_RELEASE_MASK
                            | GDK_POINTER_MOTION_MASK
                            | GDK_TOUCH_MASK
                            | GDK_ENTER_NOTIFY_MASK
                            | GDK_LEAVE_NOTIFY_MASK);

  attributes_mask = GDK_WA_X | GDK_WA_Y;

  window = gtk_widget_get_parent_window (widget);
  gtk_widget_set_window (widget, window);
  g_object_ref (window);

  priv->event_window = gdk_window_new (window, &attributes, attributes_mask);
  gtk_widget_register_window (widget, priv->event_window);

  ui_element_update_event_region (element);
}

static void
ui_element_unrealize (GtkWidget *widget)
{
  UiElement *element = UI_ELEMENT (widget);
  UiElementPrivate *priv = element->priv;

  if (priv->event_window != NULL)
    {
      gtk_widget_unregister_window (widget, priv->event_window);
      gdk_window_destroy (priv->event_window);
      priv->event_window = NULL;
    }

  GTK_WIDGET_CLASS (ui_element_parent_class)->unrealize (widget);
}

static void
ui_element_map (GtkWidget *widget)
{
  UiElement *element = UI_ELEMENT (widget);
  UiElementPrivate *priv = element->priv;

  GTK_WIDGET_CLASS (ui_element_parent_class)->map (widget);

  if (priv->event_window != NULL)
    gdk_window_show (priv->event_window);
}

static void
ui_element_unmap (GtkWidget *widget)
{
  UiElement *element = UI_ELEMENT (widget);
  UiElementPrivate *priv = element->priv;

  if (priv->event_window != NULL)
    gdk_window_hide (priv->event_window);

  GTK_WIDGET_CLASS (ui_element_parent_class)->unmap (widget);
}

static void
ui_element_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
  UiElement *element = UI_ELEMENT (widget);
  GtkWidget *child = gtk_bin_get_child (GTK_BIN (widget));

  GTK_WIDGET_CLASS (ui_element_parent_class)->size_allocate (widget,
                                                             allocation);

  if (child != NULL)
    gtk_popover_set_relative_to (GTK_POPOVER (element->priv->popover), child);

  if (gtk_widget_get_realized (widget))
    {
      gdk_window_move_resize (element->priv->event_window,
                              allocation->x,
                              allocation->y,
                              allocation->width,
                              allocation->height);

      ui_element_update_event_region (element);
    }
}

static gboolean
ui_element_draw (GtkWidget *widget, cairo_t *cr)
{
#ifdef DEBUG
  UiElement *element;
  UiElementPrivate *priv;
  GtkAllocation allocation;

  element = UI_ELEMENT (widget);
  priv = element->priv;

  cairo_set_line_width (cr, 5);
  cairo_set_source_rgba (cr, 1, 0, 0, 1);

  gtk_widget_get_allocation (widget, &allocation);
  cairo_rectangle (cr, 2, 2, allocation.width - 4, allocation.height - 4);
  cairo_stroke (cr);

  if (priv->event_region == NULL)
    return FALSE;

  cairo_set_source_rgba (cr, 1, 0, 0, 1);
  cairo_set_line_width (cr, 1);

  for (int i = 0, j = cairo_region_num_rectangles (priv->event_region); i < j; ++i)
    {
      cairo_rectangle_int_t rect;
      cairo_region_get_rectangle (priv->event_region, i, &rect);
      cairo_rectangle (cr, rect.x, rect.y, rect.width, rect.height);
      cairo_stroke (cr);
    }
#endif /* DEBUG */

  GTK_WIDGET_CLASS (ui_element_parent_class)->draw (widget, cr);

  return FALSE;
}

/******************************************************************************/
/* Helper Functions                                                           */
/******************************************************************************/

static void
ui_element_update_event_region (UiElement *element)
{
  UiElementPrivate *priv;
  UiElementClass *class;

  class = UI_ELEMENT_GET_CLASS (element);
  if (class->get_event_region == NULL)
    return;

  priv = element->priv;
  if (priv->event_window == NULL)
    return;

  if (priv->event_region != NULL)
    cairo_region_destroy (priv->event_region);

  priv->event_region = cairo_region_create ();
  class->get_event_region (element, priv->event_region);

  gdk_window_input_shape_combine_region (priv->event_window,
                                         priv->event_region,
                                         0, 0);
}

/******************************************************************************/
/* Signal Handlers                                                            */
/******************************************************************************/

static gboolean
on_button_press (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
  (void) user_data;

  UiElement *element = UI_ELEMENT (widget);
  UiElementPrivate *priv = element->priv;
  GtkAllocation allocation;

  if (!priv->is_hovering)
    return TRUE;

  priv->is_pressed = TRUE;

  gtk_widget_get_allocation (widget, &allocation);
  priv->press_x = (gdouble) allocation.x + event->x;
  priv->press_y = (gdouble) allocation.y + event->y;

  if (gtk_widget_get_focus_on_click (widget) && !gtk_widget_has_focus (widget))
    gtk_widget_grab_focus (widget);

  return FALSE;
}

static gboolean
on_button_release (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
  (void) user_data;

  UiElement *element = UI_ELEMENT (widget);
  UiElementPrivate *priv = element->priv;
  GtkAllocation allocation;
  gdouble dx, dy;

  if (!priv->is_pressed)
    return TRUE;

  priv->is_pressed = FALSE;

  gtk_widget_get_allocation (widget, &allocation);
  dx = ((gdouble) allocation.x + event->x) - priv->press_x;
  dy = ((gdouble) allocation.y + event->y) - priv->press_y;

  /* Emit click signal if cursor has moved less than 5px since press.  */
  if (sqrt ((dx  * dx) + (dy * dy)) < 5.)
    g_signal_emit (element, element_signals[CLICKED], 0);

  return FALSE;
}

static gboolean
on_motion_notify (GtkWidget *widget, GdkEventMotion *event, gpointer user_data)
{
  (void) user_data;

  UiElement *element = UI_ELEMENT (widget);
  UiElementPrivate *priv = element->priv;

  if (priv->is_pressed)
    return FALSE;

  if (!ui_element_contains_point (UI_ELEMENT (widget), event->x, event->y))
    {
      if (priv->is_hovering && !priv->is_pressed)
        {
          priv->is_hovering = FALSE;
          g_signal_emit (element, element_signals[HOVER_END], 0);
          if (priv->event_window != NULL)
            gdk_window_lower (priv->event_window);
          gtk_widget_queue_draw (widget);
        }
      return TRUE;
    }

  if (!priv->is_hovering)
    {
      priv->is_hovering = TRUE;
      g_signal_emit (element, element_signals[HOVER_START], 0);
      if (priv->event_window != NULL)
        gdk_window_raise (priv->event_window);
      gtk_widget_queue_draw (widget);
    }

  return FALSE;
}

static gboolean
on_leave_notify (GtkWidget *widget, GdkEventCrossing *event, gpointer user_data)
{
  (void) event;
  (void) user_data;

  UiElement *element = UI_ELEMENT (widget);
  UiElementPrivate *priv = element->priv;

  if (priv->is_hovering && !priv->is_pressed)
    {
      priv->is_hovering = FALSE;
      g_signal_emit (element, element_signals[HOVER_END], 0);
      if (priv->event_window != NULL)
        gdk_window_lower (priv->event_window);
      gtk_widget_queue_draw (widget);
    }

  return FALSE;
}

static void
on_clicked (UiElement *element)
{
  if (gtk_widget_is_visible (GTK_WIDGET (element->priv->popover)))
    gtk_popover_popdown (element->priv->popover);
  else if (gtk_bin_get_child (GTK_BIN (element->priv->popover)) != NULL)
    gtk_popover_popup (element->priv->popover);
}
