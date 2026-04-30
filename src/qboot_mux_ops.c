/**
 * @file qboot_mux_ops.c
 * @brief Mux backend that dispatches IO ops to FAL/FS/CUSTOM.
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

#if (QBT_BACKEND_COUNT > 1)

#define DBG_TAG "qb_mux"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#ifdef QBOOT_PKG_SOURCE_FAL
const qboot_io_ops_t *qbt_fal_io_ops(void);
const qboot_header_parser_ops_t *qbt_fal_parser_ops(void);
#endif

#ifdef QBOOT_PKG_SOURCE_FS
const qboot_io_ops_t *qbt_fs_io_ops(void);
const qboot_header_parser_ops_t *qbt_fs_parser_ops(void);
#endif

#ifdef QBOOT_PKG_SOURCE_CUSTOM
const qboot_io_ops_t *qbt_custom_io_ops(void);
const qboot_header_parser_ops_t *qbt_custom_parser_ops(void);
#endif

typedef struct
{
    qbt_store_backend_t backend;
    void *backend_handle;
} qbt_mux_handle_t;

static qbt_mux_handle_t g_mux_handles[QBOOT_TARGET_COUNT];

typedef const qboot_io_ops_t *(*qbt_mux_io_getter_t)(void);
typedef const qboot_header_parser_ops_t *(*qbt_mux_parser_getter_t)(void);

static const qbt_mux_io_getter_t g_mux_io_getters[QBT_STORE_BACKEND_COUNT] = {
#ifdef QBOOT_PKG_SOURCE_FAL
    [QBT_STORE_BACKEND_FAL] = qbt_fal_io_ops,
#endif
#ifdef QBOOT_PKG_SOURCE_FS
    [QBT_STORE_BACKEND_FS] = qbt_fs_io_ops,
#endif
#ifdef QBOOT_PKG_SOURCE_CUSTOM
    [QBT_STORE_BACKEND_CUSTOM] = qbt_custom_io_ops,
#endif
};

static const qbt_mux_parser_getter_t g_mux_parser_getters[QBT_STORE_BACKEND_COUNT] = {
#ifdef QBOOT_PKG_SOURCE_FAL
    [QBT_STORE_BACKEND_FAL] = qbt_fal_parser_ops,
#endif
#ifdef QBOOT_PKG_SOURCE_FS
    [QBT_STORE_BACKEND_FS] = qbt_fs_parser_ops,
#endif
#ifdef QBOOT_PKG_SOURCE_CUSTOM
    [QBT_STORE_BACKEND_CUSTOM] = qbt_custom_parser_ops,
#endif
};

/**
 * @brief Get IO ops table for the given backend.
 *
 * @param backend Backend selector.
 * @return IO ops table pointer.
 */
static const qboot_io_ops_t *qbt_mux_backend_ops(qbt_store_backend_t backend)
{
    return g_mux_io_getters[backend]();
}

/**
 * @brief Get parser ops table for the given backend.
 *
 * @param backend Backend selector.
 * @return Parser ops table pointer.
 */
static const qboot_header_parser_ops_t *qbt_mux_backend_parser_ops(qbt_store_backend_t backend)
{
    return g_mux_parser_getters[backend]();
}

/**
 * @brief Open a backend handle for a target descriptor.
 *
 * @param id     Target id slot for mux handle.
 * @param desc   Target descriptor.
 * @param handle Output mux handle.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
static rt_err_t qbt_mux_open_backend(qbt_target_id_t id, const qboot_store_desc_t *desc, void **handle, int flags)
{
    void *backend_handle = RT_NULL;
    if (qbt_mux_backend_ops(desc->backend)->open(id, &backend_handle, flags) != RT_EOK)
    {
        return -RT_ERROR;
    }
    g_mux_handles[id].backend = desc->backend;
    g_mux_handles[id].backend_handle = backend_handle;
    *handle = (void *)&g_mux_handles[id];
    return RT_EOK;
}

/**
 * @brief Open mux handle by target id.
 *
 * @param id     Target identifier.
 * @param handle Output mux handle.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
static rt_err_t qbt_mux_open(qbt_target_id_t id, void **handle, int flags)
{
    const qboot_store_desc_t *desc = qbt_target_desc(id);
    if (desc == RT_NULL)
    {
        return -RT_ERROR;
    }
    return qbt_mux_open_backend(id, desc, handle, flags);
}

/**
 * @brief Close mux handle.
 *
 * @param handle Mux handle.
 *
 * @return RT_EOK on success.
 */
static rt_err_t qbt_mux_close(void *handle)
{
    qbt_mux_handle_t *mux_handle = (qbt_mux_handle_t *)(handle);

    qbt_mux_backend_ops(mux_handle->backend)->close(mux_handle->backend_handle);
    return RT_EOK;
}

/**
 * @brief Read data through mux backend.
 *
 * @param handle Mux handle.
 * @param off    Byte offset.
 * @param buf    Output buffer.
 * @param len    Bytes to read.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
static rt_err_t qbt_mux_read(void *handle, rt_uint32_t off, void *buf, rt_uint32_t len)
{
    qbt_mux_handle_t *mux_handle = (qbt_mux_handle_t *)(handle);
    return qbt_mux_backend_ops(mux_handle->backend)->read(mux_handle->backend_handle, off, buf, len);
}

/**
 * @brief Erase data through mux backend.
 *
 * @param handle Mux handle.
 * @param off    Byte offset.
 * @param len    Bytes to erase.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
static rt_err_t qbt_mux_erase(void *handle, rt_uint32_t off, rt_uint32_t len)
{
    qbt_mux_handle_t *mux_handle = (qbt_mux_handle_t *)(handle);
    return qbt_mux_backend_ops(mux_handle->backend)->erase(mux_handle->backend_handle, off, len);
}

/**
 * @brief Write data through mux backend.
 *
 * @param handle Mux handle.
 * @param off    Byte offset.
 * @param buf    Input buffer.
 * @param len    Bytes to write.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
static rt_err_t qbt_mux_write(void *handle, rt_uint32_t off, const void *buf, rt_uint32_t len)
{
    qbt_mux_handle_t *mux_handle = (qbt_mux_handle_t *)(handle);
    return qbt_mux_backend_ops(mux_handle->backend)->write(mux_handle->backend_handle, off, buf, len);
}

/**
 * @brief Query size through mux backend.
 *
 * @param handle   Mux handle.
 * @param out_size Output size in bytes.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
static rt_err_t qbt_mux_size(void *handle, rt_uint32_t *out_size)
{
    qbt_mux_handle_t *mux_handle = (qbt_mux_handle_t *)(handle);
    return qbt_mux_backend_ops(mux_handle->backend)->size(mux_handle->backend_handle, out_size);
}

/**
 * @brief Issue ioctl through mux backend.
 *
 * @param handle Mux handle.
 * @param cmd    IOCTL command.
 * @param arg    Command argument.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
static rt_err_t qbt_mux_ioctl(void *handle, int cmd, void *arg)
{
    qbt_mux_handle_t *mux_handle = (qbt_mux_handle_t *)(handle);
    return qbt_mux_backend_ops(mux_handle->backend)->ioctl(mux_handle->backend_handle, cmd, arg);
}

/**
 * @brief Read release sign through mux backend.
 *
 * @param handle   Mux handle.
 * @param released Output flag, RT_TRUE when sign exists.
 * @param fw_info  Firmware header context.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
static rt_err_t qbt_mux_sign_read(void *handle, rt_bool_t *released, const fw_info_t *fw_info)
{
    qbt_mux_handle_t *mux_handle = (qbt_mux_handle_t *)(handle);
    return qbt_mux_backend_parser_ops(mux_handle->backend)->sign_read(mux_handle->backend_handle, released, fw_info);
}

/**
 * @brief Write release sign through mux backend.
 *
 * @param handle  Mux handle.
 * @param fw_info Firmware header context.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
static rt_err_t qbt_mux_sign_write(void *handle, const fw_info_t *fw_info)
{
    qbt_mux_handle_t *mux_handle = (qbt_mux_handle_t *)(handle);
    return  qbt_mux_backend_parser_ops(mux_handle->backend)->sign_write(mux_handle->backend_handle, fw_info);
}

/**
 * @brief Clear release sign through mux backend.
 *
 * @param handle  Mux handle.
 * @param fw_info Firmware header context.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
static rt_err_t qbt_mux_sign_clear(void *handle, const fw_info_t *fw_info)
{
    qbt_mux_handle_t *mux_handle = (qbt_mux_handle_t *)(handle);
    return qbt_mux_backend_parser_ops(mux_handle->backend)->sign_clear(mux_handle->backend_handle, fw_info);
}

static const qboot_io_ops_t g_qboot_io_mux = {
    .open = qbt_mux_open,
    .close = qbt_mux_close,
    .read = qbt_mux_read,
    .erase = qbt_mux_erase,
    .write = qbt_mux_write,
    .size = qbt_mux_size,
    .ioctl = qbt_mux_ioctl,
};

static const qboot_header_parser_ops_t g_qboot_header_parser_mux = {
    .sign_read = qbt_mux_sign_read,
    .sign_write = qbt_mux_sign_write,
    .sign_clear = qbt_mux_sign_clear,
};

const qboot_io_ops_t *qbt_mux_io_ops(void)
{
    return &g_qboot_io_mux;
}

const qboot_header_parser_ops_t *qbt_mux_parser_ops(void)
{
    return &g_qboot_header_parser_mux;
}

#endif /* QBT_BACKEND_COUNT > 1 */
