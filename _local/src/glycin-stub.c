/*
 * glycin-2 stub library for macOS
 * Provides no-op implementations of all glycin functions so that
 * Nautilus can link against it on macOS where the real glycin
 * cannot be built (requires memfd, seccomp, etc.).
 */

#include <glycin.h>
#include <stdlib.h>

/* ---- GlyLoader ---- */

struct _GlyLoader { GObject parent_instance; };

G_DEFINE_TYPE(GlyLoader, gly_loader, G_TYPE_OBJECT)
static void gly_loader_class_init(GlyLoaderClass *klass) { (void)klass; }
static void gly_loader_init(GlyLoader *self) { (void)self; }

/* ---- GlyImage ---- */

struct _GlyImage { GObject parent_instance; };

G_DEFINE_TYPE(GlyImage, gly_image, G_TYPE_OBJECT)
static void gly_image_class_init(GlyImageClass *klass) { (void)klass; }
static void gly_image_init(GlyImage *self) { (void)self; }

/* ---- GlyFrameRequest ---- */

struct _GlyFrameRequest { GObject parent_instance; };

G_DEFINE_TYPE(GlyFrameRequest, gly_frame_request, G_TYPE_OBJECT)
static void gly_frame_request_class_init(GlyFrameRequestClass *klass) { (void)klass; }
static void gly_frame_request_init(GlyFrameRequest *self) { (void)self; }

/* ---- GlyFrame ---- */

struct _GlyFrame { GObject parent_instance; };

G_DEFINE_TYPE(GlyFrame, gly_frame, G_TYPE_OBJECT)
static void gly_frame_class_init(GlyFrameClass *klass) { (void)klass; }
static void gly_frame_init(GlyFrame *self) { (void)self; }

/* ---- GlyNewFrame ---- */

struct _GlyNewFrame { GObject parent_instance; };

G_DEFINE_TYPE(GlyNewFrame, gly_new_frame, G_TYPE_OBJECT)
static void gly_new_frame_class_init(GlyNewFrameClass *klass) { (void)klass; }
static void gly_new_frame_init(GlyNewFrame *self) { (void)self; }

/* ---- GlyEncodedImage ---- */

struct _GlyEncodedImage { GObject parent_instance; };

G_DEFINE_TYPE(GlyEncodedImage, gly_encoded_image, G_TYPE_OBJECT)
static void gly_encoded_image_class_init(GlyEncodedImageClass *klass) { (void)klass; }
static void gly_encoded_image_init(GlyEncodedImage *self) { (void)self; }

/* ---- GlyCreator ---- */

struct _GlyCreator { GObject parent_instance; };

G_DEFINE_TYPE(GlyCreator, gly_creator, G_TYPE_OBJECT)
static void gly_creator_class_init(GlyCreatorClass *klass) { (void)klass; }
static void gly_creator_init(GlyCreator *self) { (void)self; }

/* ---- Enum GType registrations ---- */

GType gly_sandbox_selector_get_type(void)
{
    static volatile gsize g_type_id = 0;
    if (g_once_init_enter(&g_type_id)) {
        static const GEnumValue values[] = {
            { GLY_SANDBOX_SELECTOR_AUTO, "GLY_SANDBOX_SELECTOR_AUTO", "auto" },
            { GLY_SANDBOX_SELECTOR_BWRAP, "GLY_SANDBOX_SELECTOR_BWRAP", "bwrap" },
            { GLY_SANDBOX_SELECTOR_FLATPAK_SPAWN, "GLY_SANDBOX_SELECTOR_FLATPAK_SPAWN", "flatpak-spawn" },
            { GLY_SANDBOX_SELECTOR_NOT_SANDBOXED, "GLY_SANDBOX_SELECTOR_NOT_SANDBOXED", "not-sandboxed" },
            { 0, NULL, NULL }
        };
        GType t = g_enum_register_static("GlySandboxSelector", values);
        g_once_init_leave(&g_type_id, t);
    }
    return g_type_id;
}

GType gly_memory_format_selection_get_type(void)
{
    static volatile gsize g_type_id = 0;
    if (g_once_init_enter(&g_type_id)) {
        static const GFlagsValue values[] = {
            { GLY_MEMORY_SELECTION_B8G8R8A8_PREMULTIPLIED, "GLY_MEMORY_SELECTION_B8G8R8A8_PREMULTIPLIED", "b8g8r8a8-premultiplied" },
            { 0, NULL, NULL }
        };
        GType t = g_flags_register_static("GlyMemoryFormatSelection", values);
        g_once_init_leave(&g_type_id, t);
    }
    return g_type_id;
}

GType gly_memory_format_get_type(void)
{
    static volatile gsize g_type_id = 0;
    if (g_once_init_enter(&g_type_id)) {
        static const GEnumValue values[] = {
            { GLY_MEMORY_B8G8R8A8_PREMULTIPLIED, "GLY_MEMORY_B8G8R8A8_PREMULTIPLIED", "b8g8r8a8-premultiplied" },
            { 0, NULL, NULL }
        };
        GType t = g_enum_register_static("GlyMemoryFormat", values);
        g_once_init_leave(&g_type_id, t);
    }
    return g_type_id;
}

GType gly_loader_error_get_type(void)
{
    static volatile gsize g_type_id = 0;
    if (g_once_init_enter(&g_type_id)) {
        static const GEnumValue values[] = {
            { GLY_LOADER_ERROR_FAILED, "GLY_LOADER_ERROR_FAILED", "failed" },
            { GLY_LOADER_ERROR_UNKNOWN_IMAGE_FORMAT, "GLY_LOADER_ERROR_UNKNOWN_IMAGE_FORMAT", "unknown-image-format" },
            { GLY_LOADER_ERROR_NO_MORE_FRAMES, "GLY_LOADER_ERROR_NO_MORE_FRAMES", "no-more-frames" },
            { 0, NULL, NULL }
        };
        GType t = g_enum_register_static("GlyLoaderError", values);
        g_once_init_leave(&g_type_id, t);
    }
    return g_type_id;
}

/* ---- GlyCicp boxed type ---- */

GlyCicp *gly_cicp_copy(GlyCicp *cicp)
{
    if (!cicp) return NULL;
    GlyCicp *copy = g_new(GlyCicp, 1);
    *copy = *cicp;
    return copy;
}

void gly_cicp_free(GlyCicp *cicp)
{
    g_free(cicp);
}

GType gly_cicp_get_type(void)
{
    static volatile gsize g_type_id = 0;
    if (g_once_init_enter(&g_type_id)) {
        GType t = g_boxed_type_register_static("GlyCicp",
            (GBoxedCopyFunc)gly_cicp_copy,
            (GBoxedFreeFunc)gly_cicp_free);
        g_once_init_leave(&g_type_id, t);
    }
    return g_type_id;
}

/* ---- Error quark ---- */

GQuark gly_loader_error_quark(void)
{
    return g_quark_from_static_string("gly-loader-error-quark");
}

/* ---- GlyLoader functions ---- */

GlyLoader *gly_loader_new(GFile *file)
{
    (void)file;
    return g_object_new(GLY_TYPE_LOADER, NULL);
}

GlyLoader *gly_loader_new_for_stream(GInputStream *stream)
{
    (void)stream;
    return g_object_new(GLY_TYPE_LOADER, NULL);
}

GlyLoader *gly_loader_new_for_bytes(GBytes *bytes)
{
    (void)bytes;
    return g_object_new(GLY_TYPE_LOADER, NULL);
}

GStrv gly_loader_get_mime_types(void)
{
    GStrv result = g_new0(gchar*, 1);
    result[0] = NULL;
    return result;
}

void gly_loader_get_mime_types_async(GCancellable *cancellable,
                                     GAsyncReadyCallback callback,
                                     gpointer user_data)
{
    GTask *task = g_task_new(NULL, cancellable, callback, user_data);
    g_task_return_pointer(task, gly_loader_get_mime_types(), (GDestroyNotify)g_strfreev);
    g_object_unref(task);
}

GStrv gly_loader_get_mime_types_finish(GAsyncResult *result, GError **error)
{
    return g_task_propagate_pointer(G_TASK(result), error);
}

void gly_loader_set_sandbox_selector(GlyLoader *loader,
                                     GlySandboxSelector sandbox_selector)
{
    (void)loader; (void)sandbox_selector;
}

void gly_loader_set_accepted_memory_formats(GlyLoader *loader,
                                            GlyMemoryFormatSelection memory_format_selection)
{
    (void)loader; (void)memory_format_selection;
}

void gly_loader_set_apply_transformations(GlyLoader *loader,
                                          gboolean apply_transformations)
{
    (void)loader; (void)apply_transformations;
}

GlyImage *gly_loader_load(GlyLoader *loader, GError **error)
{
    (void)loader;
    g_set_error_literal(error, gly_loader_error_quark(), GLY_LOADER_ERROR_FAILED,
                        "glycin stub: loading not supported on macOS");
    return NULL;
}

void gly_loader_load_async(GlyLoader *loader, GCancellable *cancellable,
                            GAsyncReadyCallback callback, gpointer user_data)
{
    GTask *task = g_task_new(loader, cancellable, callback, user_data);
    g_task_return_new_error(task, gly_loader_error_quark(), GLY_LOADER_ERROR_FAILED,
                            "glycin stub: loading not supported on macOS");
    g_object_unref(task);
}

GlyImage *gly_loader_load_finish(GlyLoader *loader, GAsyncResult *result, GError **error)
{
    (void)loader;
    return g_task_propagate_pointer(G_TASK(result), error);
}

/* ---- GlyFrameRequest functions ---- */

GlyFrameRequest *gly_frame_request_new(void)
{
    return g_object_new(GLY_TYPE_FRAME_REQUEST, NULL);
}

void gly_frame_request_set_scale(GlyFrameRequest *frame_request,
                                 uint32_t width, uint32_t height)
{
    (void)frame_request; (void)width; (void)height;
}

void gly_frame_request_set_loop_animation(GlyFrameRequest *frame_request,
                                          gboolean loop_animation)
{
    (void)frame_request; (void)loop_animation;
}

/* ---- GlyImage functions ---- */

GlyFrame *gly_image_next_frame(GlyImage *image, GError **error)
{
    (void)image;
    g_set_error_literal(error, gly_loader_error_quark(), GLY_LOADER_ERROR_FAILED,
                        "glycin stub: not supported on macOS");
    return NULL;
}

void gly_image_next_frame_async(GlyImage *image, GCancellable *cancellable,
                                 GAsyncReadyCallback callback, gpointer user_data)
{
    GTask *task = g_task_new(image, cancellable, callback, user_data);
    g_task_return_new_error(task, gly_loader_error_quark(), GLY_LOADER_ERROR_FAILED,
                            "glycin stub: not supported on macOS");
    g_object_unref(task);
}

GlyFrame *gly_image_next_frame_finish(GlyImage *image, GAsyncResult *result, GError **error)
{
    (void)image;
    return g_task_propagate_pointer(G_TASK(result), error);
}

GlyFrame *gly_image_get_specific_frame(GlyImage *image, GlyFrameRequest *frame_request,
                                       GError **error)
{
    (void)image; (void)frame_request;
    g_set_error_literal(error, gly_loader_error_quark(), GLY_LOADER_ERROR_FAILED,
                        "glycin stub: not supported on macOS");
    return NULL;
}

void gly_image_get_specific_frame_async(GlyImage *image, GlyFrameRequest *frame_request,
                                         GCancellable *cancellable,
                                         GAsyncReadyCallback callback, gpointer user_data)
{
    (void)frame_request;
    GTask *task = g_task_new(image, cancellable, callback, user_data);
    g_task_return_new_error(task, gly_loader_error_quark(), GLY_LOADER_ERROR_FAILED,
                            "glycin stub: not supported on macOS");
    g_object_unref(task);
}

GlyFrame *gly_image_get_specific_frame_finish(GlyImage *image, GAsyncResult *result,
                                              GError **error)
{
    (void)image;
    return g_task_propagate_pointer(G_TASK(result), error);
}

const char *gly_image_get_mime_type(GlyImage *image)
{
    (void)image;
    return "application/octet-stream";
}

uint32_t gly_image_get_width(GlyImage *image)
{
    (void)image;
    return 0;
}

uint32_t gly_image_get_height(GlyImage *image)
{
    (void)image;
    return 0;
}

gchar *gly_image_get_metadata_key_value(GlyImage *image, const gchar *key)
{
    (void)image; (void)key;
    return NULL;
}

GStrv gly_image_get_metadata_keys(GlyImage *image)
{
    (void)image;
    GStrv result = g_new0(gchar*, 1);
    result[0] = NULL;
    return result;
}

uint16_t gly_image_get_transformation_orientation(GlyImage *image)
{
    (void)image;
    return 1; /* normal orientation */
}

/* ---- GlyMemoryFormat functions ---- */

gboolean gly_memory_format_has_alpha(GlyMemoryFormat memory_format)
{
    (void)memory_format;
    return FALSE;
}

gboolean gly_memory_format_is_premultiplied(GlyMemoryFormat memory_format)
{
    (void)memory_format;
    return FALSE;
}

/* ---- GlyFrame functions ---- */

int64_t gly_frame_get_delay(GlyFrame *frame)
{
    (void)frame;
    return 0;
}

uint32_t gly_frame_get_width(GlyFrame *frame)
{
    (void)frame;
    return 0;
}

uint32_t gly_frame_get_height(GlyFrame *frame)
{
    (void)frame;
    return 0;
}

uint32_t gly_frame_get_stride(GlyFrame *frame)
{
    (void)frame;
    return 0;
}

GBytes *gly_frame_get_buf_bytes(GlyFrame *frame)
{
    (void)frame;
    return g_bytes_new(NULL, 0);
}

GlyMemoryFormat gly_frame_get_memory_format(GlyFrame *frame)
{
    (void)frame;
    return GLY_MEMORY_R8G8B8A8;
}

GlyCicp *gly_frame_get_color_cicp(GlyFrame *frame)
{
    (void)frame;
    return NULL;
}

/* ---- GlyNewFrame functions ---- */

gboolean gly_new_frame_set_color_icc_profile(GlyNewFrame *new_frame, GBytes *icc_profile)
{
    (void)new_frame; (void)icc_profile;
    return FALSE;
}

/* ---- GlyEncodedImage functions ---- */

GBytes *gly_encoded_image_get_data(GlyEncodedImage *encoded_image)
{
    (void)encoded_image;
    return g_bytes_new(NULL, 0);
}

/* ---- GlyCreator functions ---- */

GlyCreator *gly_creator_new(const gchar *mime_type, GError **error)
{
    (void)mime_type;
    g_set_error_literal(error, gly_loader_error_quark(), GLY_LOADER_ERROR_FAILED,
                        "glycin stub: creation not supported on macOS");
    return NULL;
}

GlyNewFrame *gly_creator_add_frame(GlyCreator *creator, uint32_t width, uint32_t height,
                                    GlyMemoryFormat memory_format, GBytes *texture,
                                    GError **error)
{
    (void)creator; (void)width; (void)height; (void)memory_format; (void)texture;
    g_set_error_literal(error, gly_loader_error_quark(), GLY_LOADER_ERROR_FAILED,
                        "glycin stub: not supported on macOS");
    return NULL;
}

GlyNewFrame *gly_creator_add_frame_with_stride(GlyCreator *creator, uint32_t width,
                                                uint32_t height, uint32_t stride,
                                                GlyMemoryFormat memory_format, GBytes *texture,
                                                GError **error)
{
    (void)creator; (void)width; (void)height; (void)stride;
    (void)memory_format; (void)texture;
    g_set_error_literal(error, gly_loader_error_quark(), GLY_LOADER_ERROR_FAILED,
                        "glycin stub: not supported on macOS");
    return NULL;
}

GlyEncodedImage *gly_creator_create(GlyCreator *creator, GError **error)
{
    (void)creator;
    g_set_error_literal(error, gly_loader_error_quark(), GLY_LOADER_ERROR_FAILED,
                        "glycin stub: not supported on macOS");
    return NULL;
}

void gly_creator_create_async(GlyCreator *creator, GCancellable *cancellable,
                               GAsyncReadyCallback callback, gpointer user_data)
{
    GTask *task = g_task_new(creator, cancellable, callback, user_data);
    g_task_return_new_error(task, gly_loader_error_quark(), GLY_LOADER_ERROR_FAILED,
                            "glycin stub: not supported on macOS");
    g_object_unref(task);
}

GlyEncodedImage *gly_creator_create_finish(GlyCreator *creator, GAsyncResult *result,
                                            GError **error)
{
    (void)creator;
    return g_task_propagate_pointer(G_TASK(result), error);
}

gboolean gly_creator_add_metadata_key_value(GlyCreator *creator, const gchar *key,
                                            const gchar *value)
{
    (void)creator; (void)key; (void)value;
    return FALSE;
}

gboolean gly_creator_set_encoding_quality(GlyCreator *creator, uint8_t quality)
{
    (void)creator; (void)quality;
    return FALSE;
}

gboolean gly_creator_set_encoding_compression(GlyCreator *creator, uint8_t compression)
{
    (void)creator; (void)compression;
    return FALSE;
}

gboolean gly_creator_set_sandbox_selector(GlyCreator *creator,
                                          GlySandboxSelector sandbox_selector)
{
    (void)creator; (void)sandbox_selector;
    return FALSE;
}
