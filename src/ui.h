#ifndef UI_H
#define UI_H

extern osso_context_t *osso_context;

GtkWidget *ui_create_main_dialog(gpointer user_data);
void ui_refresh();
void ui_destroy();
gchar *ui_lock_dialog(GtkWidget *parent, const gchar *title,
                      osso_context_t *osso, gboolean show_wrong_code_banner);
#endif // UI_H
