/* snap-server.c
 *
 * HTTP JSON API on localhost.
 * Simple single-threaded server using GSocketService.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snap-server.h"
#include "snap-manager.h"
#include <json-glib/json-glib.h>
#include <stdio.h>
#include <string.h>

struct _SnapServer
{
    GObject           parent_instance;
    GSocketService   *service;
    gint              port;
    gboolean          running;
};

G_DEFINE_FINAL_TYPE (SnapServer, snap_server, G_TYPE_OBJECT)

static gchar *
build_json_response (gint status, const gchar *message)
{
    g_autoptr (JsonBuilder) builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "status");
    json_builder_add_int_value (builder, status);
    json_builder_set_member_name (builder, "message");
    json_builder_add_string_value (builder, message);
    json_builder_end_object (builder);

    g_autoptr (JsonGenerator) gen = json_generator_new ();
    json_generator_set_root (gen, json_builder_get_root (builder));
    return json_generator_to_data (gen, NULL);
}

static gchar *
build_http_response (gint status_code, const gchar *body)
{
    const gchar *status_text = status_code == 200 ? "OK" :
                               status_code == 404 ? "Not Found" : "Bad Request";

    return g_strdup_printf (
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n"
        "%s",
        status_code, status_text, strlen (body), body);
}

static void
handle_request (const gchar *method, const gchar *path,
                const gchar *body, GOutputStream *out)
{
    g_autofree gchar *json_body = NULL;
    gint http_status = 200;

    if (g_strcmp0 (path, "/status") == 0)
    {
        json_body = build_json_response (200, "snap server running");
    }
    else if (g_strcmp0 (path, "/stats") == 0)
    {
        SnapManager *mgr = snap_manager_get_default ();
        snap_manager_initialize (mgr, NULL);
        SnapStore *store = snap_store_get_default ();

        g_autoptr (JsonBuilder) builder = json_builder_new ();
        json_builder_begin_object (builder);
        json_builder_set_member_name (builder, "totalSnapshots");
        json_builder_add_int_value (builder, snap_store_get_total_snapshots (store));
        json_builder_set_member_name (builder, "totalFiles");
        json_builder_add_int_value (builder, snap_store_get_total_files (store));
        json_builder_set_member_name (builder, "storeSize");
        json_builder_add_int_value (builder, snap_store_get_store_size (store));
        json_builder_end_object (builder);

        g_autoptr (JsonGenerator) gen = json_generator_new ();
        json_generator_set_root (gen, json_builder_get_root (builder));
        json_body = json_generator_to_data (gen, NULL);
    }
    else
    {
        http_status = 404;
        json_body = build_json_response (404, "Not found");
    }

    g_autofree gchar *response = build_http_response (http_status, json_body);
    g_output_stream_write_all (out, response, strlen (response), NULL, NULL, NULL);
}

static gboolean
on_incoming (GSocketService    *service,
             GSocketConnection *connection,
             GObject           *source_object,
             gpointer           user_data)
{
    GInputStream *in = g_io_stream_get_input_stream (G_IO_STREAM (connection));
    GOutputStream *out = g_io_stream_get_output_stream (G_IO_STREAM (connection));

    /* Read request (simple: just first 4096 bytes) */
    gchar buffer[4096];
    gssize bytes_read = g_input_stream_read (in, buffer, sizeof (buffer) - 1,
                                             NULL, NULL);
    if (bytes_read <= 0)
        return FALSE;

    buffer[bytes_read] = '\0';

    /* Parse HTTP request line */
    gchar method[16] = {0};
    gchar path[256] = {0};
    sscanf (buffer, "%15s %255s", method, path);

    /* Find body (after \r\n\r\n) */
    const gchar *body = strstr (buffer, "\r\n\r\n");
    if (body != NULL)
        body += 4;

    handle_request (method, path, body, out);

    return FALSE;
}

static void
snap_server_finalize (GObject *object)
{
    SnapServer *self = SNAP_SERVER (object);
    snap_server_stop (self);
    g_clear_object (&self->service);
    G_OBJECT_CLASS (snap_server_parent_class)->finalize (object);
}

static void
snap_server_class_init (SnapServerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = snap_server_finalize;
}

static void
snap_server_init (SnapServer *self)
{
    self->service = g_socket_service_new ();
    self->running = FALSE;
    self->port = 7483;
}

SnapServer *
snap_server_new (void)
{
    return g_object_new (SNAP_TYPE_SERVER, NULL);
}

gboolean
snap_server_start (SnapServer *self, gint port, GError **error)
{
    g_return_val_if_fail (SNAP_IS_SERVER (self), FALSE);

    if (self->running)
        return TRUE;

    self->port = port > 0 ? port : 7483;

    if (!g_socket_listener_add_inet_port (G_SOCKET_LISTENER (self->service),
                                           self->port, NULL, error))
        return FALSE;

    g_signal_connect (self->service, "incoming", G_CALLBACK (on_incoming), self);
    g_socket_service_start (self->service);
    self->running = TRUE;

    return TRUE;
}

void
snap_server_stop (SnapServer *self)
{
    g_return_if_fail (SNAP_IS_SERVER (self));

    if (self->running)
    {
        g_socket_service_stop (self->service);
        self->running = FALSE;
    }
}

gboolean
snap_server_is_running (SnapServer *self)
{
    return self->running;
}

gint
snap_server_get_port (SnapServer *self)
{
    return self->port;
}
