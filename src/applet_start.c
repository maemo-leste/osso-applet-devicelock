#include <hildon-cp-plugin/hildon-cp-plugin-interface.h>
#include <hildon/hildon.h>
#include <gtk/gtk.h>
#include <libintl.h>
#include <string.h>
#include <locale.h>
#include <gconf/gconf-client.h>

#include <libdevlock.h>
#include <codelockui.h>

#include "ui.h"
#include "settings_database.h"

static gboolean save_to_local = TRUE;
static gboolean configured = FALSE;

static guint notifications[10];
static guint last_notification = 0;

static void
remove_gconf_notifications()
{
  int i;

  if (!configured)
    return;

  for (i = 0; notifications[i] && i < last_notification; i++)
  {
    devlock_notify_remove(notifications[i]);
    notifications[i] = 0;
  }

  configured = FALSE;
  last_notification = 0;
}

static void
applet_destroy(GtkWidget *dialog)
{
  remove_gconf_notifications();
  settings_destroy();

  if (dialog)
    ui_destroy();
}

static osso_return_t
_set_notify_key(gboolean inc_notify_index, const gchar *key, setting *s)
{
  osso_return_t rv;

  if ( !save_to_local || settings_set_value(key, s) )
    rv = OSSO_OK;
  else
    rv = OSSO_ERROR;

  if (inc_notify_index)
  {
    if (++last_notification > 9)
      last_notification = 0;
  }

  return rv;
}

static osso_return_t
_add_device_lock_notify(int inc_notify_index, autolock_notify notify_func)
{
  setting device_lock = {BOOLEAN, FALSE, 0, NULL, FALSE};
  osso_return_t rv;

  get_autolock_key(&device_lock.boolval);

  if (devlock_autorelock_notify_add(notify_func,
                                    &notifications[last_notification],
                                    "device_lock"))
  {
    rv = OSSO_OK;
  }
  else
    rv = OSSO_ERROR;

  return _set_notify_key(inc_notify_index, "device_lock", &device_lock) | rv;
}

static osso_return_t
_add_autolock_period_notify(gboolean inc_index, timeout_notify notify_func)
{
  osso_return_t rv;
  setting autolock_period = {INT, FALSE, 0, NULL, FALSE};

  get_timeout_key(&autolock_period.intval);

  if (devlock_timeout_notify_add(notify_func,
                                 &notifications[last_notification],
                                 "autolock_period"))
  {
    rv = OSSO_OK;
  }
  else
    rv = OSSO_ERROR;

  return _set_notify_key(inc_index, "autolock_period", &autolock_period) | rv;
}

static void
_device_lock_notify(gboolean val)
{
  setting device_lock = {BOOLEAN, FALSE, 0, NULL, val};

  if (settings_set_value("device_lock", &device_lock))
    ui_refresh();
}

static void
_autolock_period_notify(gint val)
{
  setting autolock_period = {BOOLEAN, FALSE, val, NULL, FALSE};

  if (settings_set_value("autolock_period", &autolock_period))
    ui_refresh();
}

static osso_return_t
get_configuration(gboolean local_save)
{
  osso_return_t rv;

  save_to_local = local_save;

  if (_add_device_lock_notify(TRUE, _device_lock_notify))
  {
    set_autolock_key(FALSE);
    _add_device_lock_notify(TRUE, _device_lock_notify);
  }

  rv = _add_autolock_period_notify(TRUE, _autolock_period_notify);

  if (rv)
  {
    set_timeout_key(10);
    rv = _add_autolock_period_notify(TRUE, _autolock_period_notify);
  }


  settings_set_value("orig_autolock_period",
                     settings_get_value("autolock_period"));
  settings_set_value("orig_device_lock", settings_get_value("device_lock"));
  configured = 1;

  return rv;
}

static osso_return_t
set_configuration()
{
  const setting *device_lock;
  const setting *autolock_period;
  osso_return_t rv = OSSO_OK;

  device_lock = settings_get_value("device_lock");

  if (!device_lock || !set_autolock_key(device_lock->boolval))
    rv = OSSO_ERROR;

  autolock_period = settings_get_value("autolock_period");

  if (!autolock_period || !set_timeout_key(autolock_period->intval))
    rv = OSSO_ERROR;

  return rv;
}

static gboolean
verify_lock_code(GtkWidget *parent)
{
  gboolean show_wrong_code_banner = FALSE;

  while (1)
  {
    gchar *code = ui_lock_dialog(parent,
                                 dgettext("osso-system-lock",
                                          "secu_application_title"),
                                 osso_context,
                                 show_wrong_code_banner);
    if (!code)
      break;

    show_wrong_code_banner = TRUE;

    if (codelock_is_passwd_correct(code))
    {
      hildon_banner_show_information(
            NULL, NULL, dgettext("osso-system-lock", "secu_info_codeaccepted"));
      g_free(code);
      return TRUE;
    }

    g_free(code);
  }

  return FALSE;
}

static gboolean
setting_changed()
{
  setting *s;
  gboolean orig_device_lock;
  gboolean rv;
  int orig_autolock_period;

  if (!(s = settings_get_value("orig_device_lock")))
    return TRUE;

  orig_device_lock = s->boolval;

  if (!(s = settings_get_value("device_lock")))
    return TRUE;

  rv = s->boolval;

  if (rv != orig_device_lock)
    return TRUE;

  if (rv)
  {
    if (!(s = settings_get_value("orig_autolock_period")))
      return TRUE;

    orig_autolock_period = s->intval;

    if (!(s = settings_get_value("autolock_period")))
      return TRUE;

    rv = !(s->intval == orig_autolock_period);
  }

  return rv;
}

osso_return_t
execute(osso_context_t *osso, gpointer user_data, gboolean user_activated)
{
  osso_return_t rv;
  GtkWidget *main_dialog = NULL;
  gboolean active = FALSE;

  setlocale(6, "");
  bindtextdomain("osso-applet-devicelock", "/usr/share/locale");
  bind_textdomain_codeset("osso-applet-devicelock", "UTF-8");
  textdomain("osso-applet-devicelock");

  settings_init(&active);

  if (!user_activated && !active)
  {
    applet_destroy(main_dialog);
    return OSSO_OK;
  }

  if (!active)
  {
    rv = get_configuration(TRUE);

    if (rv == OSSO_ERROR)
    {
      applet_destroy(main_dialog);
      return rv;
    }
  }

  osso_context = osso;

  if (!codelockui_init(osso))
    return OSSO_ERROR;

  main_dialog = ui_create_main_dialog(user_data);

  if (!main_dialog)
  {
    codelockui_deinit();
    return OSSO_ERROR;
  }

  ui_refresh();

  do
  {
    while (1)
    {
      gint resp = gtk_dialog_run(GTK_DIALOG(main_dialog));

      if (resp == GTK_RESPONSE_OK)
        break;

      if (resp != GTK_RESPONSE_OK)
      {
        applet_destroy(main_dialog);
        codelockui_deinit();
        return OSSO_OK;
      }
    }
  }
  while (setting_changed() && !verify_lock_code(NULL));

  if (setting_changed())
    rv = set_configuration();
  else
    rv = OSSO_OK;

  applet_destroy(main_dialog);
  codelockui_deinit();

  return rv;
}
