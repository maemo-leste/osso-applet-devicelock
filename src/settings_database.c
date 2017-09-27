#include <hildon-cp-plugin/hildon-cp-plugin-interface.h>
#include <gconf/gconf-client.h>
#include <gtk/gtk.h>

#include <string.h>

#include "settings_database.h"
#include "ui.h"

#define GCONF_KEY_APPLET_DEVICELOCK "/apps/osso/applet/osso-applet-devicelock"

static GConfClient *gc_client = NULL;
static GHashTable *settings;
static guint gc_notify_id = 0;
static GList *settings_keys = NULL;
static guint ui_refresh_timeout_id = 0;

static gchar *
_gconf_key(const gchar *key)
{
  return
      g_strconcat(GCONF_KEY_APPLET_DEVICELOCK, "/", key, NULL);
}

static gchar *
_gconf_to_key(const gchar *gc_key)
{
  if (g_str_has_prefix(gc_key, GCONF_KEY_APPLET_DEVICELOCK))
    return g_strdup(gc_key + strlen(GCONF_KEY_APPLET_DEVICELOCK));

  return NULL;
}

static setting *
_get_local(const gchar *key)
{
  if (settings)
    return (setting *)g_hash_table_lookup(settings, key);

  return NULL;
}

static gboolean
_gconf_val(const GConfValue *val, setting *s)
{
  switch (val->type)
  {
    case GCONF_VALUE_INT:
    {
      s->type = INT;
      s->local = FALSE;
      s->intval = gconf_value_get_int(val);
      break;
    }
    case GCONF_VALUE_BOOL:
    {
      s->type = BOOLEAN;
      s->local = FALSE;
      s->boolval = gconf_value_get_bool(val);
      break;
    }
    case GCONF_VALUE_STRING:
    {
      s->type = STRING;
      s->local = FALSE;
      s->stringval = g_strdup(gconf_value_get_string(val));
      break;
    }
    default:
      return FALSE;
  }

  return TRUE;
}

static gboolean
_set_local(const gchar *key, const setting *s)
{
  gchar *nkey = g_strdup(key);
  setting *ns;

  if (!nkey)
    return FALSE;

  ns = (setting *)g_memdup(s, 0x14u);
  if (!ns)
  {
    g_free(nkey);
    return FALSE;
  }

  if (s->type != STRING || (ns->stringval = g_strdup(s->stringval)))
  {
    g_hash_table_replace(settings, nkey, ns);
    return TRUE;
  }

  g_free(ns);
  g_free(nkey);

  return FALSE;
}

static gboolean
_set_gconf(const gchar *key, const setting *s)
{
  gchar *gckey;
  gboolean rv;

  gckey = _gconf_key(key);
  gconf_client_remove_dir(gc_client, GCONF_KEY_APPLET_DEVICELOCK, 0);

  switch (s->type)
  {
    case STRING:
    {
      rv = gconf_client_set_string(gc_client, gckey, s->stringval, 0);
      break;
    }
    case BOOLEAN:
    {
      rv = gconf_client_set_bool(gc_client, gckey, s->boolval, 0);
      break;
    }
    case INT:
    {
      rv = gconf_client_set_int(gc_client, gckey, s->intval, 0);
      break;
    }
    default:
      rv = TRUE;
  }

  gconf_client_add_dir(gc_client, GCONF_KEY_APPLET_DEVICELOCK, 0, 0);
  g_free(gckey);

  return rv;
}

static setting *
setting_create(const gchar *key)
{
  gchar *gc_key;
  GConfValue *val;
  setting s;

  gc_key = _gconf_key(key);
  val = gconf_client_get(gc_client, gc_key, 0);
  g_free(gc_key);

  if (!val)
    return NULL;

  if (_gconf_val(val, &s))
  {
    gconf_value_free(val);

    if (_set_local(key, &s))
    {

      if (s.type == STRING)
        g_free(s.stringval);

      return _get_local(key);
    }
  }
  else
    gconf_value_free(val);

  return NULL;
}

static void
setting_key_free(gpointer data)
{
  if (data)
    g_free(data);
}

static void
setting_data_free(gpointer data)
{
  setting *s = (setting *)data;

  if (s)
  {
    if (s->type == STRING && s->stringval)
      g_free(s->stringval);

    g_free(s);
  }
}

static gboolean
_ui_refresh_cb(gpointer user_data)
{
  ui_refresh_timeout_id = 0;
  ui_refresh();

  return FALSE;
}

static void
_change_in_gconf(GConfClient *client, guint cnxn_id, GConfEntry *entry,
                 gpointer user_data)
{
  GConfValue *val;
  gchar *key;
  setting s = {0, };

  g_return_if_fail(entry);
  g_return_if_fail(client);

  val = gconf_entry_get_value(entry);

  if (!val)
    return;

  key = _gconf_to_key(gconf_entry_get_key(entry));

  if ( _gconf_val(val, &s) && _set_local(key, &s) )
  {
    if (s.type == STRING)
      g_free(s.stringval);

    if (strstr(gconf_entry_get_key(entry), "appletactive") &&
        !gconf_value_get_bool(val))
    {
      ui_destroy(0);
    }
    else if (!ui_refresh_timeout_id)
      ui_refresh_timeout_id = g_timeout_add(500u, _ui_refresh_cb, NULL);
  }

  g_free(key);
}

gboolean
settings_init(gboolean *applet_active)
{
  setting *s;
  setting state;

#if !GLIB_CHECK_VERSION(2,35,0)
g_type_init ();
#endif

  gc_client = gconf_client_get_default();

  if (!gc_client)
    return FALSE;

  settings = g_hash_table_new_full(g_str_hash, g_str_equal, setting_key_free,
                                   setting_data_free);

  *applet_active = FALSE;
  settings_keys = NULL;

  s = setting_create("appletactive");

  if (s && s->type == BOOLEAN)
    *applet_active = s->boolval;

  gconf_client_add_dir(gc_client, GCONF_KEY_APPLET_DEVICELOCK,
                       GCONF_CLIENT_PRELOAD_NONE, NULL);

  state.type = BOOLEAN;
  state.boolval = TRUE;

  _set_gconf("appletactive", &state);

  gc_notify_id = gconf_client_notify_add(gc_client, GCONF_KEY_APPLET_DEVICELOCK,
                                         _change_in_gconf, NULL, NULL, NULL);

  if (!gc_notify_id)
  {
    gconf_client_remove_dir(gc_client, GCONF_KEY_APPLET_DEVICELOCK, NULL);
    g_object_unref(gc_client);
    return FALSE;
  }

  return TRUE;
}

setting *
settings_get_value(const gchar *key)
{
  setting *s = _get_local(key);

  if (!s)
    s = setting_create(key);

  return s;
}

gboolean
settings_set_value(const gchar *key, setting *s)
{
  if (g_hash_table_lookup(settings, key))
    s->local = TRUE;
  else
    s->local = FALSE;

  if (_set_local(key, s))
    return _set_gconf(key, s);

  return FALSE;
}

static void
setting_key_free_cb(gpointer key, gpointer data)
{
  setting_key_free(key);
}

gboolean
settings_destroy()
{
  setting s;

  if (!gc_client)
    return FALSE;

  if (ui_refresh_timeout_id)
  {
    g_source_remove(ui_refresh_timeout_id);
    ui_refresh_timeout_id = 0;
  }

  s.type = BOOLEAN;
  s.boolval = FALSE;
  _set_gconf("appletactive", &s);

  if (gc_notify_id)
  {
    gconf_client_notify_remove(gc_client, gc_notify_id);
    gconf_client_remove_dir(gc_client, GCONF_KEY_APPLET_DEVICELOCK, NULL);
    gc_notify_id = 0;
  }

  gconf_client_suggest_sync(gc_client, NULL);
  gconf_client_clear_cache(gc_client);
  g_object_unref(gc_client);
  gc_client = NULL;

  g_list_foreach(settings_keys, setting_key_free_cb, NULL);
  g_list_free(settings_keys);
  settings_keys = NULL;

  g_hash_table_destroy(settings);
  settings = NULL;

  return TRUE;
}

GList *
settings_get_keys()
{
  GSList *l;
  GSList *iter;

  g_list_foreach(settings_keys, setting_key_free_cb, NULL);
  g_list_free(settings_keys);
  settings_keys = NULL;
  l = gconf_client_all_entries(gc_client, GCONF_KEY_APPLET_DEVICELOCK, NULL);

  for (iter = l; iter; iter = iter->next)
  {
    GConfEntry *entry = iter->data;

    if (entry->value)
      settings_keys = g_list_append(settings_keys, _gconf_to_key(entry->key));

    ;
    gconf_entry_free(entry);
  }

  g_slist_free(l);

  return settings_keys;
}
