#ifndef SETTINGS_DATABASE_H
#define SETTINGS_DATABASE_H

enum SETTING_TYPE {
  INT,
  STRING,
  BOOLEAN
};

struct _setting
{
  enum SETTING_TYPE type;
  gboolean local;
  gint intval;
  gchar *stringval;
  gboolean boolval;
};

typedef struct _setting setting;

setting *settings_get_value(const gchar *key);
gboolean settings_set_value(const gchar *key, setting *s);
gboolean settings_init(gboolean *applet_active);
gboolean settings_destroy();
GList *settings_get_keys();

#endif // SETTINGS_DATABASE_H
