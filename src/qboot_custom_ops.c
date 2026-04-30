/**
 * @file qboot_custom_ops.c
 * @brief Custom flash-backed package source/target operations for qboot.
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

#ifdef QBOOT_PKG_SOURCE_CUSTOM

#define DBG_TAG "qb_custom"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

/**
 * @brief Weak hook for custom flash read.
 *
 * @param addr Flash address.
 * @param buf  Output buffer.
 * @param len  Bytes to read.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
rt_weak rt_err_t qbt_custom_flash_read(rt_uint32_t addr, void *buf, rt_uint32_t len)
{
    RT_UNUSED(addr);
    RT_UNUSED(buf);
    RT_UNUSED(len);
    return -RT_ERROR;
}

/**
 * @brief Weak hook for custom flash write.
 *
 * @param addr Flash address.
 * @param buf  Input buffer.
 * @param len  Bytes to write.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
rt_weak rt_err_t qbt_custom_flash_write(rt_uint32_t addr, const void *buf, rt_uint32_t len)
{
    RT_UNUSED(addr);
    RT_UNUSED(buf);
    RT_UNUSED(len);
    return -RT_ERROR;
}

/**
 * @brief Weak hook for custom flash erase.
 *
 * @param addr Flash address.
 * @param len  Bytes to erase.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
rt_weak rt_err_t qbt_custom_flash_erase(rt_uint32_t addr, rt_uint32_t len)
{
    RT_UNUSED(addr);
    RT_UNUSED(len);
    return -RT_ERROR;
}

/**
 * @brief Find custom backend descriptor by id.
 *
 * @param id Target identifier.
 * @return Descriptor pointer or RT_NULL.
 */
static const qboot_store_desc_t *qbt_custom_find_desc(qbt_target_id_t id)
{
    const qboot_store_desc_t *desc = qbt_target_desc(id);
    if (desc == RT_NULL)
    {
        return RT_NULL;
    }
    return (desc->backend == QBT_STORE_BACKEND_CUSTOM) ? desc : RT_NULL;
}

/**
 * @brief Validate custom flash range.
 *
 * @param desc Target descriptor.
 * @param off  Byte offset.
 * @param len  Bytes to access.
 *
 * @return RT_TRUE when range is valid.
 */
static rt_bool_t qbt_custom_range_check(const qboot_store_desc_t *desc, rt_uint32_t off, rt_uint32_t len)
{
    if (desc == RT_NULL || desc->flash_len == 0)
    {
        return RT_FALSE;
    }
    if (off > desc->flash_len)
    {
        return RT_FALSE;
    }
    if (len > (desc->flash_len - off))
    {
        return RT_FALSE;
    }
    return RT_TRUE;
}

/**
 * @brief Open custom backend by id.
 *
 * @param id     Target identifier.
 * @param handle Output handle.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
static rt_err_t qbt_custom_open(qbt_target_id_t id, void **handle, int flags)
{
    RT_UNUSED(flags);
    const qboot_store_desc_t *desc = qbt_custom_find_desc(id);
    if (desc == RT_NULL || desc->flash_len == 0)
    {
        LOG_E("CUSTOM open id %d fail.", id);
        return -RT_ERROR;
    }
    *handle = (void *)desc;
    return RT_EOK;
}

/**
 * @brief Close custom backend handle (no-op).
 *
 * @param handle Backend handle.
 *
 * @return RT_EOK always.
 */
static rt_err_t qbt_custom_close(void *handle)
{
    RT_UNUSED(handle);
    return RT_EOK;
}

/**
 * @brief Read from custom flash backend.
 *
 * @param handle Backend handle.
 * @param off    Byte offset.
 * @param buf    Output buffer.
 * @param len    Bytes to read.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
static rt_err_t qbt_custom_read(void *handle, rt_uint32_t off, void *buf, rt_uint32_t len)
{
    const qboot_store_desc_t *desc = (const qboot_store_desc_t *)handle;
    if (!qbt_custom_range_check(desc, off, len))
    {
        return -RT_ERROR;
    }
    return qbt_custom_flash_read(desc->flash_addr + off, buf, len);
}

/**
 * @brief Erase custom flash backend.
 *
 * @param handle Backend handle.
 * @param off    Byte offset.
 * @param len    Bytes to erase.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
static rt_err_t qbt_custom_erase(void *handle, rt_uint32_t off, rt_uint32_t len)
{
    const qboot_store_desc_t *desc = (const qboot_store_desc_t *)handle;
    if (!qbt_custom_range_check(desc, off, len))
    {
        return -RT_ERROR;
    }
    return qbt_custom_flash_erase(desc->flash_addr + off, len);
}

/**
 * @brief Write to custom flash backend.
 *
 * @param handle Backend handle.
 * @param off    Byte offset.
 * @param buf    Input buffer.
 * @param len    Bytes to write.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
static rt_err_t qbt_custom_write(void *handle, rt_uint32_t off, const void *buf, rt_uint32_t len)
{
    const qboot_store_desc_t *desc = (const qboot_store_desc_t *)handle;
    if (!qbt_custom_range_check(desc, off, len))
    {
        return -RT_ERROR;
    }
    return qbt_custom_flash_write(desc->flash_addr + off, buf, len);
}

/**
 * @brief Get custom backend size.
 *
 * @param handle   Backend handle.
 * @param out_size Output size in bytes.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
static rt_err_t qbt_custom_size(void *handle, rt_uint32_t *out_size)
{
    const qboot_store_desc_t *desc = (const qboot_store_desc_t *)handle;
    if (desc == RT_NULL || out_size == RT_NULL)
    {
        return -RT_ERROR;
    }
    *out_size = desc->flash_len;
    return RT_EOK;
}

/**
 * @brief Handle custom backend IOCTL.
 *
 * @param handle Backend handle.
 * @param cmd    IOCTL command.
 * @param arg    Command argument.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
static rt_err_t qbt_custom_ioctl(void *handle, int cmd, void *arg)
{
    RT_UNUSED(handle);
    if (cmd == QBOOT_IO_CMD_GET_ERASE_ALIGN)
    {
        if (arg == RT_NULL)
        {
            return -RT_ERROR;
        }
        *(rt_uint32_t *)arg = QBOOT_FLASH_ERASE_ALIGN;
        return RT_EOK;
    }
    return -RT_ERROR;
}

/**
 * @brief Read release sign from custom backend.
 *
 * @param handle   Backend handle.
 * @param released Output flag, RT_TRUE when sign exists.
 * @param fw_info  Firmware header context.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
static rt_err_t qbt_custom_sign_read(void *handle, rt_bool_t *released, const fw_info_t *fw_info)
{
    rt_uint32_t pos = (((qboot_src_read_pos() + fw_info->pkg_size) + (QBOOT_RELEASE_SIGN_ALIGN_SIZE - 1)) & ~(QBOOT_RELEASE_SIGN_ALIGN_SIZE - 1));
    rt_uint32_t release_sign = 0;
    if (qbt_custom_read(handle, pos, (rt_uint8_t *)&release_sign, sizeof(rt_uint32_t)) != RT_EOK)
    {
        LOG_E("CUSTOM sign read fail at pos=%u.", (unsigned int)pos);
        return -RT_ERROR;
    }
    *released = (release_sign == QBOOT_RELEASE_SIGN_WORD);
    return RT_EOK;
}

/**
 * @brief Write release sign to custom backend.
 *
 * @param handle  Backend handle.
 * @param fw_info Firmware header context.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
static rt_err_t qbt_custom_sign_write(void *handle, const fw_info_t *fw_info)
{
    rt_uint32_t release_sign = QBOOT_RELEASE_SIGN_WORD;
    rt_uint32_t pos = (((qboot_src_read_pos() + fw_info->pkg_size) + (QBOOT_RELEASE_SIGN_ALIGN_SIZE - 1)) & ~(QBOOT_RELEASE_SIGN_ALIGN_SIZE - 1));
    if (qbt_custom_write(handle, pos, (rt_uint8_t *)&release_sign, sizeof(rt_uint32_t)) != RT_EOK)
    {
        LOG_E("CUSTOM sign write fail at pos=%u.", (unsigned int)pos);
        return -RT_ERROR;
    }
    return RT_EOK;
}

/**
 * @brief Clear release sign for custom backend (no-op).
 *
 * @param handle  Backend handle.
 * @param fw_info Firmware header context.
 *
 * @return RT_EOK always.
 */
static rt_err_t qbt_custom_sign_clear(void *handle, const fw_info_t *fw_info)
{
    RT_UNUSED(handle);
    RT_UNUSED(fw_info);
    return RT_EOK;
}

static const qboot_io_ops_t g_qboot_io_custom = {
    .open = qbt_custom_open,
    .close = qbt_custom_close,
    .read = qbt_custom_read,
    .erase = qbt_custom_erase,
    .write = qbt_custom_write,
    .size = qbt_custom_size,
    .ioctl = qbt_custom_ioctl,
};

static const qboot_header_parser_ops_t g_qboot_header_parser_custom = {
    .sign_read = qbt_custom_sign_read,
    .sign_write = qbt_custom_sign_write,
    .sign_clear = qbt_custom_sign_clear,
};

const qboot_io_ops_t *qbt_custom_io_ops(void)
{
    return &g_qboot_io_custom;
}

/**
 * @brief Get custom backend parser ops table.
 *
 * @return Parser ops table pointer.
 */
const qboot_header_parser_ops_t *qbt_custom_parser_ops(void)
{
    return &g_qboot_header_parser_custom;
}

#endif /* QBOOT_PKG_SOURCE_CUSTOM */
