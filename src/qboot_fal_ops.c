/**
 * @file qboot_fal_ops.c
 * @brief FAL-backed package source/target operations for qboot.
 * @author wdfk-prog ()
 * @version 1.0
 * @date 2026-01-07
 * 
 * @copyright Copyright (c) 2026  
 * 
 * @note :
 * @par Change Log:
 * Date       Version Author      Description
 * 2026-01-07 1.0     wdfk-prog   first version
 */
#include <qboot.h>
#include <fal.h>

#ifdef QBOOT_PKG_SOURCE_FAL

#define DBG_TAG "qb_fal"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>
/**
 * @brief Open FAL partition by target id.
 *
 * @param id      Target identifier.
 * @param handle  Output partition handle.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
static rt_err_t qbt_fal_open(qbt_target_id_t id, void **handle, int flags)
{
    RT_UNUSED(flags);
    const qboot_store_desc_t *desc = qbt_target_desc(id);
    if (desc == RT_NULL)
    {
        return -RT_ERROR;
    }
    fal_partition_t part = (fal_partition_t)fal_partition_find(desc->store_name);
    if (part == RT_NULL)
    {
        LOG_E("FAL open partition %s fail.", desc->store_name);
        return -RT_ERROR;
    }
    *handle = part;
    return RT_EOK;
}

/**
 * @brief Close FAL partition handle (no-op).
 *
 * @param handle Partition handle (unused).
 *
 * @return RT_EOK always.
 */
static rt_err_t qbt_fal_close(void *handle)
{
    RT_UNUSED(handle);
    return RT_EOK;
}

/**
 * @brief Read from FAL partition.
 *
 * @param handle Partition handle.
 * @param off    Byte offset.
 * @param buf    Output buffer.
 * @param len    Bytes to read.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
static rt_err_t qbt_fal_read(void *handle, rt_uint32_t off, void *buf, rt_uint32_t len)
{
    /* 0 bytes is a no-op. */
    if(len == 0)
    {
        return RT_EOK;
    }
    if (fal_partition_read((fal_partition_t)handle, off, buf, len) < 0)
    {
        LOG_E("FAL read fail, off=%u len=%u", off, len);
        return -RT_ERROR;
    }
    return RT_EOK;
}

/**
 * @brief Erase FAL partition region.
 *
 * @param handle Partition handle.
 * @param off    Byte offset.
 * @param len    Bytes to erase.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
static rt_err_t qbt_fal_erase(void *handle, rt_uint32_t off, rt_uint32_t len)
{
    /* 0 bytes is a no-op. */
    if(len == 0)
    {
        return RT_EOK;
    }
    if (fal_partition_erase((fal_partition_t)handle, off, len) < 0)
    {
        LOG_E("FAL erase fail, off=%u len=%u", off, len);
        return -RT_ERROR;
    }
    return RT_EOK;
}

/**
 * @brief Write to FAL partition.
 *
 * @param handle Partition handle.
 * @param off    Byte offset.
 * @param buf    Input data buffer.
 * @param len    Bytes to write.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
static rt_err_t qbt_fal_write(void *handle, rt_uint32_t off, const void *buf, rt_uint32_t len)
{
    /* 0 bytes is a no-op. */
    if(len == 0)
    {
        return RT_EOK;
    }
    if (fal_partition_write((fal_partition_t)handle, off, buf, len) < 0)
    {
        LOG_E("FAL write fail, off=%u len=%u", off, len);
        return -RT_ERROR;
    }
    return RT_EOK;
}

/**
 * @brief Get FAL partition size.
 *
 * @param handle   Partition handle.
 * @param out_size Output size in bytes.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
static rt_err_t qbt_fal_size(void *handle, rt_uint32_t *out_size)
{
    *out_size = ((fal_partition_t)handle)->len;
    return RT_EOK;
}

/**
 * @brief IO control for FAL partition handles.
 *
 * @param handle Partition handle.
 * @param cmd    IO control command.
 * @param arg    Command argument pointer.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
static rt_err_t qbt_fal_ioctl(void *handle, int cmd, void *arg)
{
    fal_partition_t part = (fal_partition_t)handle;

    switch (cmd)
    {
    case QBOOT_IO_CMD_GET_ERASE_ALIGN:
    {
        *(rt_uint32_t *)arg = fal_flash_device_find(part->flash_name)->blk_size;
        return RT_EOK;
    }
    default:
        return -RT_ERROR;
    }
}

/**
 * @brief Read release sign from FAL package partition.
 *
 * @param handle    Partition handle.
 * @param released  Output flag set to true if sign matches.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
static rt_err_t qbt_fal_sign_read(void *handle, rt_bool_t *released, const fw_info_t *fw_info)
{
    fal_partition_t part = (fal_partition_t)handle;
    rt_uint32_t pos = (((qboot_src_read_pos() + fw_info->pkg_size) + (QBOOT_RELEASE_SIGN_ALIGN_SIZE - 1)) & ~(QBOOT_RELEASE_SIGN_ALIGN_SIZE - 1));
    rt_uint32_t release_sign = 0;
    if (fal_partition_read(part, pos, (rt_uint8_t *)&release_sign, sizeof(rt_uint32_t)) < 0)
    {
        LOG_E("FAL sign read fail at pos=%u.", (unsigned int)pos);
        return -RT_ERROR;
    }
    *released = (release_sign == QBOOT_RELEASE_SIGN_WORD);
    return RT_EOK;
}

static rt_err_t qbt_fal_sign_write(void *handle, const fw_info_t *fw_info)
{
    fal_partition_t part = (fal_partition_t)handle;
    rt_uint32_t release_sign = QBOOT_RELEASE_SIGN_WORD;
    rt_uint32_t pos = (((qboot_src_read_pos() + fw_info->pkg_size) + (QBOOT_RELEASE_SIGN_ALIGN_SIZE - 1)) & ~(QBOOT_RELEASE_SIGN_ALIGN_SIZE - 1));
    if (fal_partition_write(part, pos, (rt_uint8_t *)&release_sign, sizeof(rt_uint32_t)) < 0)
    {
        LOG_E("FAL sign write fail at pos=%u.", (unsigned int)pos);
        return -RT_ERROR;
    }
    return RT_EOK;
}

static rt_err_t qbt_fal_sign_clear(void *handle, const fw_info_t *fw_info)
{
    RT_UNUSED(handle);
    RT_UNUSED(fw_info);
    return RT_EOK;
}

static const qboot_io_ops_t g_qboot_io_fal = {
    .open = qbt_fal_open,
    .close = qbt_fal_close,
    .read = qbt_fal_read,
    .erase = qbt_fal_erase,
    .write = qbt_fal_write,
    .size = qbt_fal_size,
    .ioctl = qbt_fal_ioctl,
};

static const qboot_header_parser_ops_t g_qboot_header_parser_fal = {
    .sign_read = qbt_fal_sign_read,
    .sign_write = qbt_fal_sign_write,
    .sign_clear = qbt_fal_sign_clear,
};

const qboot_io_ops_t *qbt_fal_io_ops(void)
{
    return &g_qboot_io_fal;
}

const qboot_header_parser_ops_t *qbt_fal_parser_ops(void)
{
    return &g_qboot_header_parser_fal;
}

#endif /* QBOOT_PKG_SOURCE_FAL */
