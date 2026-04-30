/**
 * @file qboot_fs_ops.c
 * @brief Filesystem-backed package source/target operations for qboot.
 * @author wdfk-prog ()
 * @version 1.0
 * @date 2026-01-15
 *
 * @copyright Copyright (c) 2026
 *
 * @note :
 * @par Change Log:
 * Date       Version Author      Description
 * 2026-01-15 1.0     wdfk-prog   first version
 */
#include <qboot.h>

#ifdef QBOOT_PKG_SOURCE_FS

#include <dfs_fs.h>
#include "dfs_romfs.h"
#include <dfs_file.h>
#include <unistd.h>

#define DBG_TAG "qb_fs"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

static int g_fs_fds[QBOOT_TARGET_COUNT];

/**
 * @brief Open filesystem-backed target by id.
 *
 * @param id     Target identifier.
 * @param handle Output handle (encoded target id).
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
static rt_err_t qbt_fs_open(qbt_target_id_t id, void **handle, int flags)
{
    int fd = -1;
    const qboot_store_desc_t *desc = qbt_target_desc(id);
    int open_flags = O_RDONLY;

    if (desc == RT_NULL)
    {
        return -RT_ERROR;
    }

    if (flags & QBT_OPEN_WRITE)
    {
        open_flags = O_RDWR;
    }
    if (flags & QBT_OPEN_CREATE)
    {
        open_flags |= O_CREAT;
    }
    if (flags & QBT_OPEN_TRUNC)
    {
        open_flags |= O_TRUNC;
    }
    fd = open(desc->store_name, open_flags, 0);
    if (fd < 0)
    {
        LOG_E("FS open file %s fail.", desc->store_name);
        return -RT_ERROR;
    }
    /* Store fd+1 so fd=0 is not treated as empty slot. */
    g_fs_fds[id] = fd + 1;
    /* Add 1 so id=0 is not treated as NULL handle. */
    *handle = (void *)(uintptr_t)(id + 1);
    return RT_EOK;
}

/**
 * @brief Close filesystem handle.
 *
 * @param handle Encoded target id.
 *
 * @return RT_EOK on success.
 */
static rt_err_t qbt_fs_close(void *handle)
{
    int id = (int)((uintptr_t)handle) - 1;
    int fd = g_fs_fds[id] - 1;
    if (fd >= 0)
    {
        close(fd);
    }
    g_fs_fds[id] = 0;
    return RT_EOK;
}

/**
 * @brief Read data from filesystem target.
 *
 * @param handle Encoded target id.
 * @param off    Byte offset.
 * @param buf    Output buffer.
 * @param len    Bytes to read.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
static rt_err_t qbt_fs_read(void *handle, rt_uint32_t off, void *buf, rt_uint32_t len)
{
    int id = (int)((uintptr_t)handle) - 1;
    int fd = g_fs_fds[id] - 1;
    if (lseek(fd, (off_t)off, SEEK_SET) < 0)
    {
        return -RT_ERROR;
    }
    if (read(fd, buf, len) != (int)len)
    {
        LOG_E("FS read fail, off=%u len=%u", off, len);
        return -RT_ERROR;
    }
    return RT_EOK;
}

/**
 * @brief Erase filesystem target.
 *
 * @param handle Encoded target id.
 * @param off    Byte offset (ignored for filesystem).
 * @param len    Bytes to erase (ignored for filesystem).
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
static rt_err_t qbt_fs_erase(void *handle, rt_uint32_t off, rt_uint32_t len)
{
    RT_UNUSED(off);
    RT_UNUSED(len);
    int id = (int)((uintptr_t)handle) - 1;
    int fd = g_fs_fds[id] - 1;

    off_t offset = 0;
#if defined(QBOOT_USING_HPATCHLITE) && defined(QBOOT_HPATCH_USE_STORAGE_SWAP) && defined(QBOOT_HPATCH_SWAP_STORE_FS)
    if (id == QBOOT_TARGET_SWAP)
    {
        offset = (off_t)QBOOT_HPATCH_SWAP_FILE_SIZE;
    }
#endif
    if (ftruncate(fd, offset) != 0)
    {
        return -RT_ERROR;
    }
    return RT_EOK;
}

/**
 * @brief Write data to filesystem target.
 *
 * @param handle Encoded target id.
 * @param off    Byte offset.
 * @param buf    Input buffer.
 * @param len    Bytes to write.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
static rt_err_t qbt_fs_write(void *handle, rt_uint32_t off, const void *buf, rt_uint32_t len)
{
    int id = (int)((uintptr_t)handle) - 1;
    int fd = g_fs_fds[id] - 1;
    if (lseek(fd, (off_t)off, SEEK_SET) < 0)
    {
        return -RT_ERROR;
    }
    if (write(fd, buf, len) != (int)len)
    {
        LOG_E("FS write fail, off=%u len=%u", off, len);
        return -RT_ERROR;
    }
    return RT_EOK;
}

/**
 * @brief Get filesystem target size.
 *
 * @param handle   Encoded target id.
 * @param out_size Output size in bytes.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
static rt_err_t qbt_fs_size(void *handle, rt_uint32_t *out_size)
{
    int id = (int)((uintptr_t)handle) - 1;
    int fd = g_fs_fds[id] - 1;
    off_t cur = 0;
    off_t end = 0;

#if defined(QBOOT_USING_HPATCHLITE) && defined(QBOOT_HPATCH_USE_STORAGE_SWAP) && defined(QBOOT_HPATCH_SWAP_STORE_FS)
    if (id == QBOOT_TARGET_SWAP)
    {
        *out_size = (rt_uint32_t)QBOOT_HPATCH_SWAP_FILE_SIZE;
        return RT_EOK;
    }
#endif
    cur = lseek(fd, 0, SEEK_CUR);
    end = lseek(fd, 0, SEEK_END);
    if (cur >= 0)
    {
        lseek(fd, cur, SEEK_SET);
    }
    if (end < 0)
    {
        return -RT_ERROR;
    }
    *out_size = (rt_uint32_t)end;
    return RT_EOK;
}

/**
 * @brief IOCTL is unsupported for filesystem backend.
 *
 * @param handle Encoded target id.
 * @param cmd    IOCTL command.
 * @param arg    Command argument.
 *
 * @return -RT_ENOSYS always.
 */
static rt_err_t qbt_fs_ioctl(void *handle, int cmd, void *arg)
{
    RT_UNUSED(handle);
    RT_UNUSED(cmd);
    RT_UNUSED(arg);
    return -RT_ENOSYS;
}

/**
 * @brief Read release sign marker from filesystem.
 *
 * @param handle   Encoded target id (unused).
 * @param released Output flag, RT_TRUE when marker exists.
 * @param fw_info  Firmware header context (unused).
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
static rt_err_t qbt_fs_sign_read(void *handle, rt_bool_t *released, const fw_info_t *fw_info)
{
    int sign_fd = -1;

    RT_UNUSED(handle);
    RT_UNUSED(fw_info);
    *released = RT_FALSE;

    sign_fd = open(QBOOT_DOWNLOAD_SIGN_FILE_PATH, O_RDONLY, 0);
    if (sign_fd < 0)
    {
        return RT_EOK;
    }
    close(sign_fd);
    *released = RT_TRUE;
    return RT_EOK;
}

/**
 * @brief Write release sign marker to filesystem.
 *
 * @param handle  Encoded target id (unused).
 * @param fw_info Firmware header context (unused).
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
static rt_err_t qbt_fs_sign_write(void *handle, const fw_info_t *fw_info)
{
    int sign_fd = -1;

    RT_UNUSED(handle);
    RT_UNUSED(fw_info);
    sign_fd = open(QBOOT_DOWNLOAD_SIGN_FILE_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0);
    if (sign_fd < 0)
    {
        LOG_E("FS sign open fail %s.", QBOOT_DOWNLOAD_SIGN_FILE_PATH);
        return -RT_ERROR;
    }
    close(sign_fd);
    return RT_EOK;
}

/**
 * @brief Clear release sign marker from filesystem.
 *
 * @param handle  Encoded target id (unused).
 * @param fw_info Firmware header context (unused).
 *
 * @return RT_EOK always.
 */
static rt_err_t qbt_fs_sign_clear(void *handle, const fw_info_t *fw_info)
{
    RT_UNUSED(handle);
    RT_UNUSED(fw_info);
    unlink(QBOOT_DOWNLOAD_SIGN_FILE_PATH);
    return RT_EOK;
}

static const qboot_io_ops_t g_qboot_io_fs = {
    .open = qbt_fs_open,
    .close = qbt_fs_close,
    .read = qbt_fs_read,
    .erase = qbt_fs_erase,
    .write = qbt_fs_write,
    .size = qbt_fs_size,
    .ioctl = qbt_fs_ioctl,
};

static const qboot_header_parser_ops_t g_qboot_header_parser_fs = {
    .sign_read = qbt_fs_sign_read,
    .sign_write = qbt_fs_sign_write,
    .sign_clear = qbt_fs_sign_clear,
};

const qboot_io_ops_t *qbt_fs_io_ops(void)
{
    return &g_qboot_io_fs;
}

const qboot_header_parser_ops_t *qbt_fs_parser_ops(void)
{
    return &g_qboot_header_parser_fs;
}

#endif /* QBOOT_PKG_SOURCE_FS */
