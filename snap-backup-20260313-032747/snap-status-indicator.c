/* snap-status-indicator.c
 *
 * Returns a GIcon representing the snap status of a file:
 *   - Green  "snap-tracked-symbolic"  if tracked and not modified
 *   - Orange "snap-modified-symbolic" if tracked and modified
 *   - NULL   if not tracked
 */

#include "snap-status-indicator.h"
#include "snap-manager.h"

GIcon *
snap_status_indicator_get_icon (const gchar *file_path)
{
    SnapManager *manager;

    g_return_val_if_fail (file_path != NULL, NULL);

    manager = snap_manager_get_default ();
    if (manager == NULL)
        return NULL;

    if (!snap_manager_is_tracked (manager, file_path))
        return NULL;

    if (snap_manager_is_modified (manager, file_path))
        return g_themed_icon_new ("snap-modified-symbolic");

    return g_themed_icon_new ("snap-tracked-symbolic");
}

gboolean
snap_status_indicator_is_tracked (const gchar *file_path)
{
    SnapManager *manager;

    g_return_val_if_fail (file_path != NULL, FALSE);

    manager = snap_manager_get_default ();
    if (manager == NULL)
        return FALSE;

    return snap_manager_is_tracked (manager, file_path);
}
