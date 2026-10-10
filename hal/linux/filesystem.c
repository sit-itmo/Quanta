#include "filesystem.h"
#include "quanta_hal_internal.h"
#include <sys/statvfs.h>

quanta_status quanta_hal_fs_refresh(quanta_filesystem_snapshot *out, quanta_hal_context context)
{
    struct statvfs st;
    uint64_t size;
    (void)context;
    /* Native statvfs reads the root filesystem, not a fixture root. */
    if (statvfs("/", &st))
    {
        out->status = quanta_errno_status(errno);
        return QUANTA_OK;
    }
    size = (uint64_t)(st.f_frsize ? st.f_frsize : st.f_bsize);
    if (!size || (uint64_t)st.f_blocks > UINT64_MAX / size ||
        (uint64_t)st.f_bavail > (uint64_t)st.f_blocks ||
        (uint64_t)st.f_bfree > (uint64_t)st.f_blocks)
    {
        out->status = QUANTA_INVALID_DATA;
        return QUANTA_OK;
    }
    out->total = (uint64_t)st.f_blocks * size;
    out->free = (uint64_t)st.f_bfree * size;
    out->available = (uint64_t)st.f_bavail * size;
    out->status = QUANTA_OK;
    return QUANTA_OK;
}

void quanta_hal_fs_cleanup(quanta_filesystem_snapshot *d)
{
    (void)d;
}
