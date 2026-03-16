/*
 * glycin-gtk4-2 stub library for macOS
 * Provides a no-op implementation of gly_gtk_frame_get_texture.
 */

#include <glib-object.h>
#include <gtk/gtk.h>
#include <stdint.h>

/* Forward-declare GlyFrame (defined in the glycin-2 stub) */
typedef struct _GlyFrame GlyFrame;

GdkTexture *gly_gtk_frame_get_texture(GlyFrame *frame)
{
    (void)frame;
    /* Return a minimal 1x1 empty texture as fallback */
    GBytes *bytes = g_bytes_new("\0\0\0\0", 4);
    GdkTexture *texture = gdk_texture_new_from_bytes(bytes, NULL);
    g_bytes_unref(bytes);
    if (texture)
        return texture;
    /* If that failed, return NULL */
    return NULL;
}
