/*  toggle_fullscreen was removed to not depend on the renderer. 
    better solution is pending invention.
   
    nice clues at
    http://stackoverflow.com/questions/263/gtk-implementation-of-messagebox 
*/

#include <gtk/gtk.h>

bool gtk_alert(const char *text, const char *caption, bool yesno) {
    // Have X, will dialog
    GtkWidget *dialog = gtk_message_dialog_new(NULL,
        GtkDialogFlags(0), // or GTK_DIALOG_MODAL. there is no parent window.
        yesno ? GTK_MESSAGE_QUESTION : GTK_MESSAGE_ERROR,
        yesno ? GTK_BUTTONS_YES_NO : GTK_BUTTONS_OK,
        "%s", text);
    gtk_window_set_position((GtkWindow*)dialog, GTK_WIN_POS_CENTER_ALWAYS);
    gtk_window_set_title((GtkWindow*)dialog, caption);
    gint dialog_ret = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    while (gtk_events_pending())
	gtk_main_iteration();

    if (yesno) {
	switch (dialog_ret) {
	    default:
	    case GTK_RESPONSE_DELETE_EVENT:
	    case GTK_RESPONSE_NO:
		return false;
	    case GTK_RESPONSE_YES:
		return true;
	}
    }
}

