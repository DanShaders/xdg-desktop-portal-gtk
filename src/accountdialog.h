#include <gtk/gtk.h>

#define ACCOUNT_TYPE_DIALOG (account_dialog_get_type())
#define ACCOUNT_DIALOG(object) \
    (G_TYPE_CHECK_INSTANCE_CAST(object, ACCOUNT_TYPE_DIALOG, AccountDialog))

typedef struct _AccountDialog AccountDialog;
typedef struct _AccountDialogClass AccountDialogClass;

AccountDialog* account_dialog_new(char const* app_id, char const* user_name, char const* real_name,
                                  char const* icon_file, char const* reason);

char const* account_dialog_get_user_name(AccountDialog* dialog);
char const* account_dialog_get_real_name(AccountDialog* dialog);
char const* account_dialog_get_icon_file(AccountDialog* dialog);
