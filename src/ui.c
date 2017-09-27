#include <hildon/hildon.h>
#include <libintl.h>
#include <gconf/gconf-client.h>

#include <codelockui.h>

#include "settings_database.h"

osso_context_t *osso_context = NULL;

static GtkWidget *dialog = NULL;
static GtkWidget *autolock_picker_button = NULL;
static gulong autolock_picker_handler = 0;

void
ui_destroy()
{
  if (dialog)
  {
    gtk_widget_destroy(dialog);
    dialog = NULL;
  }
}

void
ui_refresh()
{
  setting *device_lock;
  setting *autolock_period;
  gint index;

  if (!dialog)
    return;

  device_lock = settings_get_value("device_lock");

  if (!device_lock || device_lock->type != BOOLEAN)
  {
    index = 0;
    settings_get_value("autolock_period");
  }
  else
  {
    autolock_period = settings_get_value("autolock_period");

    if (device_lock->boolval && autolock_period && autolock_period->type == INT)
    {
      switch (autolock_period->intval)
      {
        case 5:
        {
          index = 1;
          break;
        }
        case 10:
        {
          index = 2;
          break;
        }
        case 30:
        {
          index = 3;
          break;
        }
        case 60:
        {
          index = 4;
          break;
        }
        default:
        {
          index = 0;
          break;
        }
      }
    }
  }

  g_signal_handler_block(autolock_picker_button, autolock_picker_handler);
  hildon_picker_button_set_active(
        HILDON_PICKER_BUTTON(autolock_picker_button), index);
  g_signal_handler_unblock(autolock_picker_button, autolock_picker_handler);

  return;

}

static osso_return_t
_mark_codelock_password_as_changed()
{
  GConfClient *gc = gconf_client_get_default();

  if (gc)
  {
    gconf_client_set_bool(
          gc, "/system/osso/dsm/locks/devicelock_password_changed", TRUE, NULL);
    gconf_client_suggest_sync(gc, NULL);
    g_object_unref(gc);
    return OSSO_OK;
  }

  return OSSO_ERROR;
}

static void
change_code_button_clicked(GtkButton *button, gpointer user_data)
{
  CodeLockUI ui;

  codelock_create_dialog_help(&ui, osso_context, -1, 0);
  codelock_set_max_code_length(&ui, 10);

  if (ui.dialog)
  {
    if (gtk_widget_get_toplevel(GTK_WIDGET(button)))
    {
      gtk_window_set_transient_for(
            GTK_WINDOW(ui.dialog),
            GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(button))));
    }
  }

  if (codelock_password_change(&ui, NULL))
    _mark_codelock_password_as_changed();

  codelock_destroy_dialog(&ui);
}

static void
autolock_picker_value_changed(HildonPickerButton *widget, gpointer user_data)
{
  setting device_lock;
  setting autolock_period;

  if (GTK_IS_WIDGET(user_data))
  {
    autolock_period.type = INT;
    device_lock.type = BOOLEAN;

    switch (hildon_picker_button_get_active(HILDON_PICKER_BUTTON(widget)))
    {
      case 0:
        device_lock.boolval = FALSE;
        autolock_period.intval = 1000;
        break;
      case 1:
        device_lock.boolval = TRUE;
        autolock_period.intval = 5;
        break;
      case 2:
        device_lock.boolval = TRUE;
        autolock_period.intval = 10;
        break;
      case 3:
        device_lock.boolval = TRUE;
        autolock_period.intval = 30;
        break;
      case 4:
        device_lock.boolval = TRUE;
        autolock_period.intval = 60;
        break;
    }

    if (settings_set_value("device_lock", &device_lock) )
      settings_set_value("autolock_period", &autolock_period);

    ui_refresh();
  }
}

GtkWidget *
ui_create_main_dialog(gpointer user_data)
{
  GtkWidget *autolock_touch_selector;
  GtkWidget *change_code_button;
  GtkWidget *align;
  GtkWidget *vbox;

  dialog =
      hildon_dialog_new_with_buttons(
        dgettext("osso-system-lock", "secu_security_dialog_title"),
        GTK_WINDOW(user_data),
        GTK_DIALOG_NO_SEPARATOR|GTK_DIALOG_DESTROY_WITH_PARENT|GTK_DIALOG_MODAL,
        NULL);

  if (!dialog)
    return NULL;

  hildon_dialog_add_button(HILDON_DIALOG(dialog),
                           dgettext("hildon-libs", "wdgt_bd_done"),
                           GTK_RESPONSE_OK);

  autolock_touch_selector = hildon_touch_selector_new_text();

  hildon_touch_selector_append_text(
        HILDON_TOUCH_SELECTOR(autolock_touch_selector),
        dgettext("osso-system-lock", "secu_va_disabled"));
  hildon_touch_selector_append_text(
        HILDON_TOUCH_SELECTOR(autolock_touch_selector),
        dgettext("osso-system-lock", "secu_va_5minutes"));
  hildon_touch_selector_append_text(
        HILDON_TOUCH_SELECTOR(autolock_touch_selector),
        dgettext("osso-system-lock", "secu_va_10minutes"));
  hildon_touch_selector_append_text(
        HILDON_TOUCH_SELECTOR(autolock_touch_selector),
        dgettext("osso-system-lock", "secu_va_30minutes"));
  hildon_touch_selector_append_text(
        HILDON_TOUCH_SELECTOR(autolock_touch_selector),
        dgettext("osso-system-lock", "secu_va_1hour"));
  hildon_touch_selector_set_active(
        HILDON_TOUCH_SELECTOR(autolock_touch_selector), 0, 0);

  autolock_picker_button = hildon_picker_button_new(HILDON_SIZE_FINGER_HEIGHT,
                                 HILDON_BUTTON_ARRANGEMENT_HORIZONTAL);
  hildon_button_set_title(
        HILDON_BUTTON(autolock_picker_button),
        dgettext("osso-system-lock", "secu_security_ti_autolock"));
  hildon_button_set_alignment(HILDON_BUTTON(autolock_picker_button),
                              0.0, 0.5, 1.0, 1.0);

  gtk_button_set_focus_on_click(GTK_BUTTON(autolock_picker_button), FALSE);
  GTK_WIDGET_UNSET_FLAGS(autolock_picker_button, GTK_CAN_FOCUS);
  hildon_picker_button_set_selector(
        HILDON_PICKER_BUTTON(autolock_picker_button),
        HILDON_TOUCH_SELECTOR(autolock_touch_selector));

  change_code_button = hildon_button_new_with_text(
        HILDON_SIZE_FINGER_HEIGHT, HILDON_BUTTON_ARRANGEMENT_HORIZONTAL,
        dgettext("osso-system-lock",
                 "secu_security_autolock_dialog_change_lock_code"),
        NULL);
  hildon_button_set_alignment(HILDON_BUTTON(change_code_button),
                              0.0, 0.5, 1.0, 1.0);
  gtk_button_set_focus_on_click(GTK_BUTTON(change_code_button), FALSE);
  GTK_WIDGET_UNSET_FLAGS(change_code_button, GTK_CAN_FOCUS);

  align = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  vbox = gtk_vbox_new(0, 0);
  gtk_container_add(GTK_CONTAINER(vbox), autolock_picker_button);
  gtk_container_add(GTK_CONTAINER(vbox), change_code_button);
  gtk_container_add(GTK_CONTAINER(align), vbox);
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), align);

  g_signal_connect(G_OBJECT(change_code_button), "clicked",
                   G_CALLBACK(change_code_button_clicked), dialog);
  autolock_picker_handler =
      g_signal_connect(G_OBJECT(autolock_picker_button), "value-changed",
                       G_CALLBACK(autolock_picker_value_changed), dialog);

  gtk_widget_show_all(dialog);

  return dialog;
}

static void
show_incorrect_code_banner(GtkWidget *parent)
{
  hildon_banner_show_information(parent, NULL,
                                 dgettext("osso-system-lock",
                                          "secu_info_incorrectcode"));
}

gchar *
ui_lock_dialog(GtkWidget *parent, const gchar *title, osso_context_t *osso,
               gboolean show_wrong_code_banner)
{
  gchar *code = NULL;
  GtkWidget *widget;
  CodeLockUI ui;

  widget = codelock_create_dialog_help(&ui, osso, -1, FALSE);
  codelock_set_max_code_length(&ui, 10);

  if (ui.dialog && parent)
    gtk_window_set_transient_for(GTK_WINDOW(ui.dialog), GTK_WINDOW(parent));

  if (title)
    gtk_window_set_title(GTK_WINDOW(ui.dialog), title);

  if (show_wrong_code_banner)
  {
    g_signal_connect(G_OBJECT(widget), "map-event",
                     G_CALLBACK(show_incorrect_code_banner), NULL);
  }


  if (gtk_dialog_run(GTK_DIALOG(ui.dialog)) == GTK_RESPONSE_OK)
    code = g_strdup(codelock_get_code(&ui));

  gtk_widget_hide_all(widget);
  codelock_destroy_dialog(&ui);

  return code;
}
