/**
 * @file qboot_ops.c
 * @brief 
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
#include "qboot_ops.h"

#define DBG_TAG "qb_ops"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#ifdef QBOOT_PKG_SOURCE_FAL
#include <fal.h>
#endif

const qboot_header_parser_ops_t *_header_parser_ops = RT_NULL;
const qboot_io_ops_t *_header_io_ops = RT_NULL;

#if defined(QBOOT_APP_STORE_FAL)
#define QBT_APP_BACKEND    QBT_STORE_BACKEND_FAL
#define QBT_APP_STORE_NAME QBOOT_APP_FAL_PART_NAME
#define QBT_APP_FLASH_ADDR 0u
#define QBT_APP_FLASH_LEN  0u
#elif defined(QBOOT_APP_STORE_FS)
#define QBT_APP_BACKEND    QBT_STORE_BACKEND_FS
#define QBT_APP_STORE_NAME QBOOT_APP_FILE_PATH
#define QBT_APP_FLASH_ADDR 0u
#define QBT_APP_FLASH_LEN  0u
#elif defined(QBOOT_APP_STORE_CUSTOM)
#define QBT_APP_BACKEND    QBT_STORE_BACKEND_CUSTOM
#define QBT_APP_STORE_NAME QBOOT_APP_PART_NAME
#define QBT_APP_FLASH_ADDR QBOOT_APP_FLASH_ADDR
#define QBT_APP_FLASH_LEN  QBOOT_APP_FLASH_LEN
#else
#error "APP storage backend must be selected."
#endif

#if defined(QBOOT_DOWNLOAD_STORE_FAL)
#define QBT_DOWNLOAD_BACKEND    QBT_STORE_BACKEND_FAL
#define QBT_DOWNLOAD_STORE_NAME QBOOT_DOWNLOAD_FAL_PART_NAME
#define QBT_DOWNLOAD_FLASH_ADDR 0u
#define QBT_DOWNLOAD_FLASH_LEN  0u
#elif defined(QBOOT_DOWNLOAD_STORE_FS)
#define QBT_DOWNLOAD_BACKEND    QBT_STORE_BACKEND_FS
#define QBT_DOWNLOAD_STORE_NAME QBOOT_DOWNLOAD_FILE_PATH
#define QBT_DOWNLOAD_FLASH_ADDR 0u
#define QBT_DOWNLOAD_FLASH_LEN  0u
#elif defined(QBOOT_DOWNLOAD_STORE_CUSTOM)
#define QBT_DOWNLOAD_BACKEND    QBT_STORE_BACKEND_CUSTOM
#define QBT_DOWNLOAD_STORE_NAME QBOOT_DOWNLOAD_PART_NAME
#define QBT_DOWNLOAD_FLASH_ADDR QBOOT_DOWNLOAD_FLASH_ADDR
#define QBT_DOWNLOAD_FLASH_LEN  QBOOT_DOWNLOAD_FLASH_LEN
#else
#error "DOWNLOAD storage backend must be selected."
#endif

#if defined(QBOOT_FACTORY_STORE_FAL)
#define QBT_FACTORY_BACKEND    QBT_STORE_BACKEND_FAL
#define QBT_FACTORY_STORE_NAME QBOOT_FACTORY_FAL_PART_NAME
#define QBT_FACTORY_FLASH_ADDR 0u
#define QBT_FACTORY_FLASH_LEN  0u
#elif defined(QBOOT_FACTORY_STORE_FS)
#define QBT_FACTORY_BACKEND    QBT_STORE_BACKEND_FS
#define QBT_FACTORY_STORE_NAME QBOOT_FACTORY_FILE_PATH
#define QBT_FACTORY_FLASH_ADDR 0u
#define QBT_FACTORY_FLASH_LEN  0u
#elif defined(QBOOT_FACTORY_STORE_CUSTOM)
#define QBT_FACTORY_BACKEND    QBT_STORE_BACKEND_CUSTOM
#define QBT_FACTORY_STORE_NAME QBOOT_FACTORY_PART_NAME
#define QBT_FACTORY_FLASH_ADDR QBOOT_FACTORY_FLASH_ADDR
#define QBT_FACTORY_FLASH_LEN  QBOOT_FACTORY_FLASH_LEN
#else
#error "FACTORY storage backend must be selected."
#endif

#if defined(QBOOT_USING_HPATCHLITE) && defined(QBOOT_HPATCH_USE_STORAGE_SWAP)
#if defined(QBOOT_HPATCH_SWAP_STORE_FAL)
#define QBT_SWAP_BACKEND    QBT_STORE_BACKEND_FAL
#define QBT_SWAP_STORE_NAME QBOOT_HPATCH_SWAP_PART_NAME
#define QBT_SWAP_FLASH_ADDR 0u
#define QBT_SWAP_FLASH_LEN  0u
#elif defined(QBOOT_HPATCH_SWAP_STORE_CUSTOM)
#define QBT_SWAP_BACKEND    QBT_STORE_BACKEND_CUSTOM
#define QBT_SWAP_STORE_NAME QBOOT_HPATCH_SWAP_PART_NAME
#define QBT_SWAP_FLASH_ADDR QBOOT_HPATCH_SWAP_FLASH_ADDR
#define QBT_SWAP_FLASH_LEN  QBOOT_HPATCH_SWAP_FLASH_LEN
#elif defined(QBOOT_HPATCH_SWAP_STORE_FS)
#define QBT_SWAP_BACKEND    QBT_STORE_BACKEND_FS
#define QBT_SWAP_STORE_NAME QBOOT_HPATCH_SWAP_FILE_PATH
#define QBT_SWAP_FLASH_ADDR 0u
#define QBT_SWAP_FLASH_LEN  0u
#else
#error "HPatchLite swap backend must be selected."
#endif
#endif

static const qboot_store_desc_t g_descs[QBOOT_TARGET_COUNT] = {
#define QBOOT_TARGET_DESC_INIT(id, name) \
    { QBOOT_TARGET_##id, name, QBT_##id##_STORE_NAME, QBT_##id##_FLASH_ADDR, QBT_##id##_FLASH_LEN, QBT_##id##_BACKEND },
    QBT_TARGET_LIST(QBOOT_TARGET_DESC_INIT)
#undef QBOOT_TARGET_DESC_INIT
};

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

#if (QBT_BACKEND_COUNT > 1)
const qboot_io_ops_t *qbt_mux_io_ops(void);
const qboot_header_parser_ops_t *qbt_mux_parser_ops(void);
#endif

static rt_err_t qbt_register_ops(const qboot_io_ops_t *io_ops, const qboot_header_parser_ops_t *parser_ops)
{
    if (io_ops == RT_NULL || parser_ops == RT_NULL)
    {
        return -RT_ERROR;
    }
    rt_err_t rst = qboot_register_header_io_ops(io_ops);
    if (rst != RT_EOK)
    {
        LOG_E("Register header IO ops fail: %d", rst);
        return rst;
    }
    rst = qboot_register_header_parser_ops(parser_ops);
    if (rst != RT_EOK)
    {
        LOG_E("Register header parser ops fail: %d", rst);
        return rst;
    }
    return RT_EOK;
}

/**
 * @brief Register header parser/package source operations.
 *
 * @param ops       Operation table; required fields must be non-NULL.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
rt_err_t qboot_register_header_parser_ops(const qboot_header_parser_ops_t *ops)
{
    if ((ops == RT_NULL) || (ops->sign_read == RT_NULL) || (ops->sign_write == RT_NULL) || (ops->sign_clear == RT_NULL))
    {
        return -RT_ERROR;
    }
    _header_parser_ops = ops;
    return RT_EOK;
}

/**
 * @brief Register header/package source IO operations.
 *
 * @param ops IO operation table; required fields must be non-NULL.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
rt_err_t qboot_register_header_io_ops(const qboot_io_ops_t *ops)
{
    if ((ops == RT_NULL) || (ops->open == RT_NULL) || (ops->close == RT_NULL) || (ops->read == RT_NULL) || (ops->erase == RT_NULL) || (ops->write == RT_NULL) || (ops->size == RT_NULL))
    {
        return -RT_ERROR;
    }
    _header_io_ops = ops;
    return RT_EOK;
}

/**
 * @brief Read firmware header from head or tail of target.
 *
 * @param handle   Target handle from qbt_target_open.
 * @param part_len Target total size (required when @p from_tail is RT_TRUE).
 * @param fw_info  [out] Firmware header buffer.
 * @param from_tail RT_TRUE to read at tail, RT_FALSE at offset 0.
 *
 * @return RT_TRUE on success, RT_FALSE on failure.
 */
rt_bool_t qbt_fw_info_read(void *handle, rt_uint32_t part_len, fw_info_t *fw_info, rt_bool_t from_tail)
{
    if (from_tail && part_len < qboot_src_read_pos())
    {
        LOG_E("Qboot read firmware info fail. part size %u < hdr size.", (unsigned int)part_len);
        return RT_FALSE;
    }
    rt_uint32_t addr = from_tail ? (part_len - qboot_src_read_pos()) : 0;
    if (_header_io_ops->read(handle, addr, (rt_uint8_t *)fw_info, qboot_src_read_pos()) != RT_EOK)
    {
        return (RT_FALSE);
    }
    return (RT_TRUE);
}

/**
 * @brief Write firmware header to head or tail of target.
 *
 * @param handle   Target handle from qbt_target_open.
 * @param part_len Target total size (required when @p to_tail is RT_TRUE).
 * @param fw_info  Firmware header to write.
 * @param to_tail  RT_TRUE to write at tail, RT_FALSE at offset 0.
 *
 * @return RT_TRUE on success, RT_FALSE on failure.
 */
rt_bool_t qbt_fw_info_write(void *handle, rt_uint32_t part_len, fw_info_t *fw_info, rt_bool_t to_tail)
{
    rt_uint32_t addr = to_tail ? (part_len - qboot_src_read_pos()) : 0;
    if (_header_io_ops->write(handle, addr, (rt_uint8_t *)fw_info, qboot_src_read_pos()) != RT_EOK)
    {
        return (RT_FALSE);
    }
    return (RT_TRUE);
}

/**
 * @brief Check release sign from target.
 *
 * @param handle Target handle.
 * @param name   Target name for log output.
 * @param fw_info Firmware header context.
 *
 * @return RT_TRUE when released sign is present, RT_FALSE otherwise.
 */
rt_weak rt_bool_t qbt_release_sign_check(void *handle, const char *name, fw_info_t *fw_info)
{
    rt_bool_t released = RT_FALSE;
    rt_err_t rst = _header_parser_ops->sign_read(handle, &released, fw_info);
    if (rst != RT_EOK)
    {
        LOG_E("Qboot read release sign fail from %s partition. rst=%d", name, rst);
        return (RT_FALSE);
    }
    return released;
}

/**
 * @brief Write release sign to target.
 *
 * @param handle Target handle.
 * @param name   Target name for log output.
 * @param fw_info Firmware header context.
 *
 * @return RT_TRUE on success, RT_FALSE on failure.
 */
rt_bool_t qbt_release_sign_write(void *handle, const char *name, fw_info_t *fw_info)
{
    rt_err_t rst = _header_parser_ops->sign_write(handle, fw_info);
    if (rst != RT_EOK)
    {
        LOG_E("Qboot write release sign fail from %s partition. rst=%d", name, rst);
        return (RT_FALSE);
    }
    return RT_TRUE;
}

/**
 * @brief Clear release sign from target.
 *
 * @param handle Target handle.
 * @param name   Target name for log output.
 * @param fw_info Firmware header context.
 *
 * @return RT_TRUE on success, RT_FALSE on failure.
 */
rt_bool_t qbt_release_sign_clear(void *handle, const char *name, fw_info_t *fw_info)
{
    rt_err_t rst = _header_parser_ops->sign_clear(handle, fw_info);
    if (rst != RT_EOK)
    {
        LOG_E("Qboot clear release sign fail from %s partition. rst=%d", name, rst);
        return (RT_FALSE);
    }
    return RT_TRUE;
}
/**
 * @brief Convert target name to target ID.
 *
 * @param name Target name.
 * @return Target ID or QBOOT_TARGET_COUNT when not found.
 */
qbt_target_id_t qbt_name_to_id(const char *name)
{
#define QBOOT_TARGET_MATCH(id, role) \
    if (rt_strcmp(name, role) == 0)  \
    {                                \
        return QBOOT_TARGET_##id;    \
    }
    QBT_TARGET_LIST(QBOOT_TARGET_MATCH)
#undef QBOOT_TARGET_MATCH
    return QBOOT_TARGET_COUNT;
}

/**
 * @brief Get target descriptor by id.
 *
 * @param id Target identifier.
 * @return Descriptor pointer, or RT_NULL when id is invalid.
 */
const qboot_store_desc_t *qbt_target_desc(qbt_target_id_t id)
{
    if (id >= QBOOT_TARGET_COUNT)
    {
        return RT_NULL;
    }
    return &g_descs[id];
}

/**
 * @brief Open target by id and optionally query size.
 *
 * @param id       Target identifier.
 * @param handle   Output opaque handle.
 * @param out_size Output total size; ignored if NULL.
 *
 * @return RT_TRUE on success, RT_FALSE otherwise.
 */
rt_bool_t qbt_target_open(qbt_target_id_t id, void **handle, rt_uint32_t *out_size, int flags)
{
    if (_header_io_ops->open(id, handle, flags) != RT_EOK)
    {
        return RT_FALSE;
    }
    if (out_size != RT_NULL)
    {
        if (_header_io_ops->size(*handle, out_size) != RT_EOK)
        {
            _header_io_ops->close(*handle);
            return RT_FALSE;
        }
    }
    return RT_TRUE;
}

/**
 * @brief Close target handle opened by qbt_target_open.
 *
 * @param handle Target handle.
 */
void qbt_target_close(void *handle)
{
    if (handle != RT_NULL)
    {
        _header_io_ops->close(handle);
    }
}

/**
 * @brief  Register update ops.
 * @note   This is a weak hook, override in backend implementations.
 * @retval RT_TRUE when update ops registered successfully.
 */
rt_weak rt_bool_t qbt_ops_custom_init(void)
{
    return RT_TRUE;
}

/**
 * @brief Feed watchdog (weak hook).
 */
rt_weak void qbt_wdt_feed(void)
{
#ifdef PKG_USING_SYSWATCH
    extern void syswatch_wdt_feed(void);
    syswatch_wdt_feed();
#endif /* PKG_USING_SYSWATCH */
}

/**
 * @brief  Should enter update mode?
 * @note   This is a weak hook, override in backend implementations.
 * @retval RT_TRUE when update mode should be entered.
 */
rt_weak rt_bool_t qboot_should_enter_update(void)
{
    return RT_TRUE;
}

/**
 * @brief  Get source read position.
 * @note   This is a weak hook, override in backend implementations.
 * @retval Source read position.
 */
rt_weak rt_int32_t qboot_src_read_pos(void)
{
    return sizeof(fw_info_t);
}

/**
 * @brief  Notify update result.
 * @note   This is a weak hook, override in backend implementations.
 * @param  success: RT_TRUE when update success, RT_FALSE otherwise.
 * @retval None
 */
rt_weak void qboot_notify_update_result(rt_bool_t success)
{
    RT_UNUSED(success);
}

/**
 * @brief Erase target region and feed watchdog before/after.
 *
 * @note The implementation of the "erase" operation might be blocking and the 
 * waiting time could be quite long. Therefore, it is necessary to feed the dog
 * before and after the operation.
 * @param handle Target handle.
 * @param off    Byte offset to erase.
 * @param len    Bytes to erase.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
rt_err_t qbt_erase_with_feed(void *handle, rt_uint32_t off, rt_uint32_t len)
{
    qbt_wdt_feed();
    rt_err_t rst = _header_io_ops->erase(handle, off, len);
    qbt_wdt_feed();
    return rst;
}

/**
 * @brief Register storage ops based on enabled backends.
 * @return RT_EOK on success, negative error code otherwise.
 */
rt_err_t qboot_register_storage_ops(void)
{
#if (QBT_BACKEND_COUNT == 0)
    LOG_E("No storage backend enabled.");
    return -RT_ERROR;
#elif (QBT_BACKEND_COUNT == 1)
#ifdef QBOOT_PKG_SOURCE_FAL
    if (fal_init() <= 0)
    {
        LOG_E("Qboot initialize fal fail.");
        return -RT_ERROR;
    }
    return qbt_register_ops(qbt_fal_io_ops(), qbt_fal_parser_ops());
#elif defined(QBOOT_PKG_SOURCE_FS)
    return qbt_register_ops(qbt_fs_io_ops(), qbt_fs_parser_ops());
#elif defined(QBOOT_PKG_SOURCE_CUSTOM)
    return qbt_register_ops(qbt_custom_io_ops(), qbt_custom_parser_ops());
#else
    return -RT_ERROR;
#endif
#else
#ifdef QBOOT_PKG_SOURCE_FAL
    if (fal_init() <= 0)
    {
        LOG_E("Qboot initialize fal fail.");
        return -RT_ERROR;
    }
#endif
    if (qbt_ops_custom_init() == RT_FALSE)
    {
        LOG_E("Qboot initialize custom ops fail.");
        return -RT_ERROR;
    }
    return qbt_register_ops(qbt_mux_io_ops(), qbt_mux_parser_ops());
#endif
}
