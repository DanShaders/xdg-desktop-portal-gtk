#define _GNU_SOURCE 1

#include "email.h"

#include <errno.h>
#include <fcntl.h>
#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <gtk/gtk.h>
#include <locale.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "config.h"
#include "request.h"
#include "shell-dbus.h"
#include "utils.h"
#include "xdg-desktop-portal-dbus.h"

static gboolean compose_mail_mailto(GAppInfo* info, char const* const* addrs, char const* const* cc,
                                    char const* const* bcc, char const* subject, char const* body,
                                    char const** attachments, GError** error) {
    g_autofree char* enc_subject = NULL;
    g_autofree char* enc_body = NULL;
    g_autoptr(GString) url = NULL;
    g_autoptr(GList) uris = NULL;
    int i;
    gboolean success;
    char const* sep;

    enc_subject = g_uri_escape_string(subject ? subject : "", NULL, FALSE);
    enc_body = g_uri_escape_string(body ? body : "", NULL, FALSE);

    url = g_string_new("mailto:");

    sep = "?";

    if (addrs) {
        for (i = 0; addrs[i]; i++) {
            if (i > 0) {
                g_string_append(url, ",");
            }
            g_string_append_printf(url, "%s", addrs[i]);
        }
    }

    if (cc) {
        g_string_append_printf(url, "%scc=", sep);
        sep = "&";

        for (i = 0; cc[i]; i++) {
            if (i > 0) {
                g_string_append(url, ",");
            }
            g_string_append_printf(url, "%s", cc[i]);
        }
    }

    if (bcc) {
        g_string_append_printf(url, "%sbcc=", sep);
        sep = "&";

        for (i = 0; bcc[i]; i++) {
            if (i > 0) {
                g_string_append(url, ",");
            }
            g_string_append_printf(url, "%s", bcc[i]);
        }
    }

    g_string_append_printf(url, "%ssubject=%s", sep, enc_subject);
    g_string_append_printf(url, "&body=%s", enc_body);

    for (i = 0; attachments[i]; i++) {
        g_string_append_printf(url, "&attachment=%s", attachments[i]);
    }

    uris = g_list_append(uris, url->str);

    g_debug("Launching: %s\n", url->str);

    success = g_app_info_launch_uris(info, uris, NULL, error);

    return success;
}

static gboolean compose_mail(char const* const* addrs, char const* const* cc,
                             char const* const* bcc, char const* subject, char const* body,
                             char const** attachments, GError** error) {
    g_autoptr(GAppInfo) info = NULL;

    info = g_app_info_get_default_for_uri_scheme("mailto");
    if (info == NULL) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "No mailto handler");
        return FALSE;
    }

    return compose_mail_mailto(info, addrs, cc, bcc, subject, body, attachments, error);
}

static gboolean handle_compose_email(XdpImplEmail* object, GDBusMethodInvocation* invocation,
                                     char const* arg_handle, char const* arg_app_id,
                                     char const* arg_parent_window, GVariant* arg_options) {
    g_autoptr(Request) request = NULL;
    char const* sender;
    g_autoptr(GError) error = NULL;
    char const* address = NULL;
    char const* subject = NULL;
    char const* body = NULL;
    char const* no_att[1] = {NULL};
    char const** attachments = no_att;
    guint response = 0;
    GVariantBuilder opt_builder;
    char const* const* addresses = NULL;
    char const* const* cc = NULL;
    char const* const* bcc = NULL;
    g_auto(GStrv) addrs = NULL;

    sender = g_dbus_method_invocation_get_sender(invocation);

    request = request_new(sender, arg_app_id, arg_handle);

    g_variant_lookup(arg_options, "address", "&s", &address);
    g_variant_lookup(arg_options, "addresses", "^a&s", &addresses);
    g_variant_lookup(arg_options, "cc", "^a&s", &cc);
    g_variant_lookup(arg_options, "bcc", "^a&s", &bcc);
    g_variant_lookup(arg_options, "subject", "&s", &subject);
    g_variant_lookup(arg_options, "body", "&s", &body);
    g_variant_lookup(arg_options, "attachments", "^a&s", &attachments);

    if (address) {
        if (addresses) {
            gsize len = g_strv_length((char**) addresses);
            addrs = g_new(char*, len + 2);
            addrs[0] = g_strdup(address);
            memcpy(addrs + 1, addresses, sizeof(char*) * (len + 1));
        }

        else {
            addrs = g_new(char*, 2);
            addrs[0] = g_strdup(address);
            addrs[1] = NULL;
        }
    } else {
        addrs = g_strdupv((char**) addresses);
    }

    if (!compose_mail((char const* const*) addrs, cc, bcc, subject, body, attachments, NULL)) {
        response = 2;
    }

    g_variant_builder_init(&opt_builder, G_VARIANT_TYPE_VARDICT);
    xdp_impl_email_complete_compose_email(object, invocation, response,
                                          g_variant_builder_end(&opt_builder));

    return TRUE;
}

gboolean email_init(GDBusConnection* bus, GError** error) {
    GDBusInterfaceSkeleton* helper;

    helper = G_DBUS_INTERFACE_SKELETON(xdp_impl_email_skeleton_new());

    g_signal_connect(helper, "handle-compose-email", G_CALLBACK(handle_compose_email), NULL);

    if (!g_dbus_interface_skeleton_export(helper, bus, DESKTOP_PORTAL_OBJECT_PATH, error)) {
        return FALSE;
    }

    g_debug("providing %s", g_dbus_interface_skeleton_get_info(helper)->name);

    return TRUE;
}
