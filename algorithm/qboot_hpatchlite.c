/**
 * @file qboot_hpatchlite.c
 * @brief In-place OTA update implementation using HPatchLite.
 * @author wdfk-prog ()
 * @version 1.4
 * @date 2025-09-29
 * 
 * @copyright Copyright (c) 2025  
 * 
 * @note Supports both FLASH swap and RAM buffer strategies via Kconfig.
 *       This implementation is designed for in-place updates where the old firmware
 *       partition is directly overwritten to become the new firmware partition.
 * 
 * @par Modification log:
 * Date       Version Author      Description
 * 2025-09-25 1.0     wdfk-prog     First version with flash swap.
 * 2025-09-26 1.1     wdfk-prog     Refactored to support both RAM and FLASH buffer strategies.
 * 2025-09-27 1.2     wdfk-prog     Refined with Kconfig integration, ULOG, Doxygen, and bug fixes.
 * 2025-09-29 1.4     wdfk-prog     Completed RAM buffer strategy based on user's unified logic.
 * 2026-01-21 1.5     wdfk-prog     Unify the logic of SWAP operations.
 */
#include <qboot.h>

// Only compile this file if HPatchLite is enabled
#ifdef QBOOT_USING_HPATCHLITE

#include "hpatch_impl.h"

// Define ULOG tag and level
#define DBG_TAG "qboot.hpatch"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

typedef struct hpatchi_instance_t hpatchi_instance_t;

typedef struct
{
    const char *name;                                                        /**< Swap backend name (for logs). */
    uint8_t *ram_buf;                                                        /**< Swap buffer (RAM mode) or flash copy buffer (FLASH mode). */
    int ram_buf_size;                                                        /**< Buffer size for RAM or flash copy. */
    int write_pos;                                                           /**< Current write position within the swap buffer. */
    int capacity;                                                            /**< Total capacity of the swap buffer. */
#if defined(QBOOT_HPATCH_USE_STORAGE_SWAP)
    void *part;                                                              /**< Swap buffer handle */
    int part_size;                                                           /**< Total usable size of the swap buffer area */
#endif

    hpi_BOOL (*init)(hpatchi_instance_t *instance);                          /**< Prepare swap backend. */
    void (*deinit)(hpatchi_instance_t *instance);                            /**< Cleanup swap backend. */
    hpi_BOOL (*append)(hpatchi_instance_t *instance, const hpi_byte *data,
                       hpi_size_t size);                                     /**< Append data to swap. */
    hpi_BOOL (*copy_to_old)(hpatchi_instance_t *instance, int size);         /**< Copy swap data to old partition. */
    hpi_BOOL (*reset)(hpatchi_instance_t *instance);                         /**< Reset swap state after commit. */
} qbt_swap_t;

/**
 * @brief Instance structure to hold state during the patching process.
 *        Conditionally includes members based on the selected buffer strategy.
 */
struct hpatchi_instance_t
{
    hpatchi_listener_t parent;              /**< HPatchLite listener callbacks */
    int patch_file_offset;                  /**< Starting offset of the patch data within the patch partition */
    int patch_file_len;                     /**< Total length of the patch data */
    int patch_read_pos;                     /**< Current read position within the patch data stream */
    int newer_file_len;                     /**< Expected final length of the new firmware */
    int newer_write_pos;                    /**< Logical current write position in the new firmware */
    int progress_percent;                   /**< Last reported progress percentage to avoid duplicate logs */

    void *patch_part;                       /**< Patch data handle */
    void *old_part;                         /**< Old firmware handle (the one being updated) */
    const char *patch_name;                 /**< Patch data name (for logs) */
    const char *old_name;                   /**< Old firmware name (for logs) */

    qbt_swap_t *swap;                       /**< Swap backend */
    int committed_len;                      /**< Total length of data already committed from RAM/Flash to the old partition */
    rt_uint32_t old_part_sector_size;       /**< Erase alignment size for old partition */
    rt_uint32_t old_part_erased_end;        /**< Next erase-aligned offset to erase in old partition */
};

// -----------------------------------------------------------------------------
// Common Listener Callbacks
// -----------------------------------------------------------------------------
/**
 * @brief HPatchLite listener: Reads patch data as a stream.
 * @param input_stream  Handle to the instance, used to track stream position.
 * @param data          Pointer to the buffer to fill with patch data.
 * @param size          Pointer to the requested data size (input) and actual read size (output).
 * @return hpi_TRUE on success, hpi_FALSE on failure.
 */
static hpi_BOOL _do_read_patch(hpi_TInputStreamHandle input_stream, hpi_byte *data, hpi_size_t *size)
{
    hpatchi_instance_t *instance = (hpatchi_instance_t *)input_stream;

    //If it has already been read or out of bounds, return 0 bytes directly.
    if (instance->patch_read_pos >= instance->patch_file_len)
    {
        *size = 0;
        return hpi_TRUE;
    }

    // Calculate the remaining length and adjust the length of this read
    hpi_size_t remaining_len = instance->patch_file_len - instance->patch_read_pos;
    if (*size > remaining_len)
    {
        *size = remaining_len;
    }

    //If *size is adjusted to 0, it indicates that reading is complete.
    if (*size == 0)
    {
        return hpi_TRUE;
    }

    if (_header_io_ops->read(instance->patch_part, instance->patch_file_offset + instance->patch_read_pos, data, *size) != RT_EOK)
    {
        LOG_E("Failed to read patch data from '%s'.", instance->patch_name);
        *size = 0;
        return hpi_FALSE;
    }
    instance->patch_read_pos += *size;
    return hpi_TRUE;
}

/**
 * @brief HPatchLite listener: Reads old data directly from the old partition.
 *        This function's behavior is IDENTICAL for both buffer strategies.
 * @param listener  Pointer to the listener instance.
 * @param addr      Absolute address within the old firmware to read from.
 * @param data      Pointer to the buffer to fill with old data.
 * @param size      Number of bytes to read.
 * @return hpi_TRUE on success, hpi_FALSE on failure.
 */
static hpi_BOOL _do_read_old(struct hpatchi_listener_t *listener, hpi_pos_t addr, hpi_byte *data, hpi_size_t size)
{
    hpatchi_instance_t *instance = (hpatchi_instance_t *)listener;
    if (_header_io_ops->read(instance->old_part, addr, data, size) != RT_EOK)
    {
        LOG_E("Failed to read old data from '%s'.", instance->old_name);
        return hpi_FALSE;
    }
    return hpi_TRUE;
}

// -----------------------------------------------------------------------------
// FLASH Swap Strategy Implementation
// -----------------------------------------------------------------------------
#if defined(QBOOT_HPATCH_USE_STORAGE_SWAP)

/**
 * @brief Initialize FLASH swap backend.
 *
 * @param instance Patch instance.
 * @return hpi_TRUE on success, hpi_FALSE on failure.
 */
static hpi_BOOL qbt_flash_swap_init(hpatchi_instance_t *instance)
{
    rt_uint32_t swap_part_size = 0;
    qbt_target_id_t swap_id = qbt_name_to_id(QBOOT_HPATCH_SWAP_PART_NAME);

    if (swap_id >= QBOOT_TARGET_COUNT || !qbt_target_open(swap_id, &instance->swap->part, &swap_part_size, QBT_OPEN_WRITE | QBT_OPEN_CREATE))
    {
        LOG_E("Swap partition '%s' open fail.", QBOOT_HPATCH_SWAP_PART_NAME);
        return hpi_FALSE;
    }

    if ((rt_uint32_t)QBOOT_HPATCH_SWAP_OFFSET >= swap_part_size)
    {
        LOG_E("Swap offset %d out of range (size %u).", QBOOT_HPATCH_SWAP_OFFSET, swap_part_size);
        qbt_target_close(instance->swap->part);
        return hpi_FALSE;
    }
    instance->swap->part_size = (int)(swap_part_size - (rt_uint32_t)QBOOT_HPATCH_SWAP_OFFSET);
    instance->swap->capacity = instance->swap->part_size;

    LOG_I("Erasing swap area '%s' (size: %d) before use...", QBOOT_HPATCH_SWAP_PART_NAME, instance->swap->part_size);
    if (qbt_erase_with_feed(instance->swap->part, QBOOT_HPATCH_SWAP_OFFSET, instance->swap->part_size) != RT_EOK)
    {
        LOG_E("Failed to erase swap partition '%s'!", QBOOT_HPATCH_SWAP_PART_NAME);
        qbt_target_close(instance->swap->part);
        return hpi_FALSE;
    }
    instance->swap->write_pos = 0;
    return hpi_TRUE;
}

/**
 * @brief Deinitialize FLASH swap backend.
 *
 * @param instance Patch instance.
 */
static void qbt_flash_swap_deinit(hpatchi_instance_t *instance)
{
    qbt_target_close(instance->swap->part);
    instance->swap->part = RT_NULL;
}

/**
 * @brief Commits the buffered data from the swap partition to the old partition.
 *        This involves erasing the target area on the old partition, copying data,
 *        and then erasing the swap partition for the next round.
 * @param instance Pointer to the patch instance.
 * @return hpi_TRUE on success, hpi_FALSE on failure.
 */
static hpi_BOOL qbt_flash_copy_to_old(hpatchi_instance_t *instance, int size)
{
    int remaining_len = size;
    int current_src_offset = QBOOT_HPATCH_SWAP_OFFSET;
    int current_dst_offset = instance->committed_len;

    while (remaining_len > 0)
    {
        int chunk_size = (remaining_len > instance->swap->ram_buf_size) ? instance->swap->ram_buf_size : remaining_len;

        if (_header_io_ops->read(instance->swap->part, current_src_offset, instance->swap->ram_buf, chunk_size) != RT_EOK)
        {
            return hpi_FALSE;
        }

        if (_header_io_ops->write(instance->old_part, current_dst_offset, instance->swap->ram_buf, chunk_size) != RT_EOK)
        {
            return hpi_FALSE;
        }

        current_src_offset += chunk_size;
        current_dst_offset += chunk_size;
        remaining_len -= chunk_size;
    }
    return hpi_TRUE;
}

/**
 * @brief Append data to FLASH swap.
 *
 * @param instance Patch instance.
 * @param data     Data to append.
 * @param size     Bytes to append.
 * @return hpi_TRUE on success, hpi_FALSE on failure.
 */
static hpi_BOOL qbt_flash_swap_append(hpatchi_instance_t *instance, const hpi_byte *data, hpi_size_t size)
{
    if (_header_io_ops->write(instance->swap->part, QBOOT_HPATCH_SWAP_OFFSET + instance->swap->write_pos, (const uint8_t *)data, size) != RT_EOK)
    {
        return hpi_FALSE;
    }
    instance->swap->write_pos += size;
    return hpi_TRUE;
}

/**
 * @brief Reset FLASH swap backend state.
 *
 * @param instance Patch instance.
 * @return hpi_TRUE on success, hpi_FALSE on failure.
 */
static hpi_BOOL qbt_flash_swap_reset(hpatchi_instance_t *instance)
{
    if (qbt_erase_with_feed(instance->swap->part, QBOOT_HPATCH_SWAP_OFFSET, instance->swap->part_size) != RT_EOK)
    {
        return hpi_FALSE;
    }
    instance->swap->write_pos = 0;
    return hpi_TRUE;
}

static qbt_swap_t g_qbt_swap_ops_flash = {
    .name = "FLASH",
    .ram_buf_size = QBOOT_HPATCH_COPY_BUFFER_SIZE,
    .init = qbt_flash_swap_init,
    .deinit = qbt_flash_swap_deinit,
    .append = qbt_flash_swap_append,
    .copy_to_old = qbt_flash_copy_to_old,
    .reset = qbt_flash_swap_reset,
};

#endif // QBOOT_HPATCH_USE_STORAGE_SWAP

// -----------------------------------------------------------------------------
// RAM Buffer Strategy Implementation
// -----------------------------------------------------------------------------
#if defined(QBOOT_HPATCH_USE_RAM_BUFFER)

/**
 * @brief Initialize RAM swap backend.
 *
 * @param instance Patch instance.
 * @return hpi_TRUE on success, hpi_FALSE on failure.
 */
static hpi_BOOL qbt_ram_swap_init(hpatchi_instance_t *instance)
{
    instance->swap->write_pos = 0;
    instance->swap->capacity = instance->swap->ram_buf_size;
    return hpi_TRUE;
}

/**
 * @brief Deinitialize RAM swap backend.
 *
 * @param instance Patch instance.
 */
static void qbt_ram_swap_deinit(hpatchi_instance_t *instance)
{
    RT_UNUSED(instance);
}

/**
 * @brief Commits the buffered data from the RAM buffer to the old partition.
 *        This is the core of the "write-buffer, commit-full" logic for RAM.
 * @param instance Pointer to the patch instance.
 * @return hpi_TRUE on success, hpi_FALSE on failure.
 */
static hpi_BOOL qbt_ram_copy_to_old(hpatchi_instance_t *instance, int size)
{
    return (_header_io_ops->write(instance->old_part, instance->committed_len, instance->swap->ram_buf, size) == RT_EOK);
}

/**
 * @brief Append data to RAM swap.
 *
 * @param instance Patch instance.
 * @param data     Data to append.
 * @param size     Bytes to append.
 * @return hpi_TRUE on success, hpi_FALSE on failure.
 */
static hpi_BOOL qbt_ram_swap_append(hpatchi_instance_t *instance, const hpi_byte *data, hpi_size_t size)
{
    rt_memcpy(instance->swap->ram_buf + instance->swap->write_pos, data, size);
    instance->swap->write_pos += size;
    return hpi_TRUE;
}

/**
 * @brief Reset RAM swap backend state.
 *
 * @param instance Patch instance.
 * @return hpi_TRUE on success, hpi_FALSE on failure.
 */
static hpi_BOOL qbt_ram_swap_reset(hpatchi_instance_t *instance)
{
    instance->swap->write_pos = 0;
    return hpi_TRUE;
}

static qbt_swap_t g_qbt_swap_ops_ram = {
    .name = "RAM",
    .ram_buf_size = QBOOT_HPATCH_RAM_BUFFER_SIZE,
    .init = qbt_ram_swap_init,
    .deinit = qbt_ram_swap_deinit,
    .append = qbt_ram_swap_append,
    .copy_to_old = qbt_ram_copy_to_old,
    .reset = qbt_ram_swap_reset,
};
#endif // QBOOT_HPATCH_USE_RAM_BUFFER

/**
 * @brief Allocate the swap buffer for the selected backend.
 *
 * @param instance Patch instance.
 * @return hpi_TRUE on success, hpi_FALSE on failure.
 */
static hpi_BOOL qbt_swap_alloc_buf(hpatchi_instance_t *instance)
{
    instance->swap->ram_buf = rt_malloc(instance->swap->ram_buf_size);
    if (instance->swap->ram_buf == RT_NULL)
    {
        LOG_E("Failed to malloc %d bytes for %s buffer.", instance->swap->ram_buf_size, instance->swap->name);
        return hpi_FALSE;
    }
    return hpi_TRUE;
}

/**
 * @brief Free the swap buffer for the selected backend.
 *
 * @param instance Patch instance.
 */
static void qbt_swap_free_buf(hpatchi_instance_t *instance)
{
    rt_free(instance->swap->ram_buf);
    instance->swap->ram_buf = RT_NULL;
}

/**
 * @brief  Allocate the swap buffer for the selected backend.
 * @note   The swap buffer is used to store the decompressed data.
 * @param  *instance: 
 * @param  start: 
 * @param  end: 
 * @param  align_start_up: 
 * @retval 
 */
static hpi_BOOL qbt_erase_aligned_range(hpatchi_instance_t *instance, rt_uint32_t start, rt_uint32_t end, rt_bool_t align_start_up)
{
    if (end <= start)
    {
        return hpi_TRUE;
    }

    rt_uint32_t sector_size = instance->old_part_sector_size;
    rt_uint32_t erase_start = align_start_up ? RT_ALIGN(start, sector_size) : RT_ALIGN_DOWN(start, sector_size);
    rt_uint32_t erase_end   = RT_ALIGN(end, sector_size);

    if (erase_end <= instance->old_part_erased_end)
    {
        return hpi_TRUE;
    }
    if (erase_start < instance->old_part_erased_end)
    {
        erase_start = instance->old_part_erased_end;
    }
    if (erase_end > erase_start)
    {
        rt_uint32_t erase_len = erase_end - erase_start;
        if (qbt_erase_with_feed(instance->old_part, erase_start, erase_len) != RT_EOK)
        {
            LOG_E("Swap %s erase failed for '%s' at offset %u.", instance->swap->name, instance->old_name, erase_start);
            return hpi_FALSE;
        }
        instance->old_part_erased_end = erase_end;
    }
    return hpi_TRUE;
}

/**
 * @brief Commit buffered data using swap backend.
 *
 * @param instance Patch instance.
 * @return hpi_TRUE on success, hpi_FALSE on failure.
 */
static hpi_BOOL qbt_swap_commit(hpatchi_instance_t *instance)
{
    const qbt_swap_t *ops = instance->swap;
    int used = ops->write_pos;

    if (used == 0)
    {
        return hpi_TRUE;
    }

    if (instance->progress_percent >= 0 && instance->progress_percent < 100)
    {
        rt_kprintf("\n");
    }
    LOG_I("\nCommitting %d bytes from %s swap to '%s' partition...", used, ops->name, instance->old_name);
    if (!qbt_erase_aligned_range(instance, (rt_uint32_t)instance->committed_len, (rt_uint32_t)(instance->committed_len + used), RT_FALSE))
    {
        return hpi_FALSE;
    }

    if (!ops->copy_to_old(instance, used))
    {
        LOG_E("Swap %s copy to '%s' failed.", ops->name, instance->old_name);
        return hpi_FALSE;
    }

    instance->committed_len += used;
    LOG_I("\nCommit successful. Total committed: %d bytes.", instance->committed_len);

    if (!ops->reset(instance))
    {
        LOG_E("Swap %s reset failed.", ops->name);
        return hpi_FALSE;
    }
    return hpi_TRUE;
}

/**
 * @brief HPatchLite listener: Writes new data via the configured swap backend.
 *
 * @param listener  Pointer to the listener instance.
 * @param data      Pointer to the new data to be written.
 * @param size      Size of the new data.
 * @return hpi_TRUE on success, hpi_FALSE on failure.
 */
static hpi_BOOL _do_write_new(struct hpatchi_listener_t *listener, const hpi_byte *data, hpi_size_t size)
{
    hpatchi_instance_t *instance = (hpatchi_instance_t *)listener;
    const hpi_byte *data_cur = data;
    hpi_size_t remain_size = size;
    const qbt_swap_t *ops = instance->swap;

    while (remain_size > 0)
    {
        int capacity = ops->capacity;
        int used = ops->write_pos;
        int free_space = capacity - used;
        hpi_size_t chunk = (remain_size < (hpi_size_t)free_space) ? remain_size : (hpi_size_t)free_space;

        if (chunk > 0)
        {
            if (!ops->append(instance, data_cur, chunk))
            {
                LOG_E("Write error.");
                return hpi_FALSE;
            }
            data_cur += chunk;
            remain_size -= chunk;
        }

        if (ops->write_pos >= capacity)
        {
            if (!qbt_swap_commit(instance))
            {
                return hpi_FALSE;
            }
        }
    }

    instance->newer_write_pos += size;

    int percent = instance->newer_write_pos * 100 / instance->newer_file_len;
    if (percent != instance->progress_percent && (percent % 5 == 0))
    {
        rt_kprintf("\rBuffering... %3d%%", percent);
        instance->progress_percent = percent;
        if (percent >= 100)
        {
            rt_kprintf("\n");
        }
    }
    return hpi_TRUE;
}

/**
 * @brief Performs an in-place differential update from a patch package.
 * 
 * This function orchestrates the entire patch process. It initializes the appropriate
 * buffer strategy (Flash or RAM), invokes the HPatchLite library, and handles
 * the finalization steps like committing the last block and erasing tail data.
 * 
 * @param patch_part        Handle containing the patch data.
 * @param old_part          Handle containing the old firmware, which will be updated in-place.
 * @param patch_name        Patch source name (for logs).
 * @param old_name          Old firmware name (for logs).
 * @param patch_file_len    Length of the patch data within the patch partition.
 * @param newer_file_len    Expected length of the new firmware after patching.
 * @param patch_file_offset Starting offset of the patch data within the patch partition.
 * @return 0 on success, non-zero on failure.
 */
int qbt_hpatchlite_release_from_part(void *patch_part, void *old_part, const char *patch_name, const char *old_name, int patch_file_len, int newer_file_len, int patch_file_offset)
{
    hpatchi_instance_t instance = {
        .patch_part = patch_part,
        .old_part = old_part,
        .patch_name = patch_name,
        .old_name = old_name,
        .patch_file_offset = patch_file_offset,
        .patch_file_len = patch_file_len,
        .newer_file_len = newer_file_len,
        .progress_percent = -1,
        .old_part_erased_end = 0,
#if defined(QBOOT_HPATCH_USE_STORAGE_SWAP)
        .swap = &g_qbt_swap_ops_flash,
#elif defined(QBOOT_HPATCH_USE_RAM_BUFFER)
        .swap = &g_qbt_swap_ops_ram,
#else
#error "No HPatchLite buffer strategy selected. Please define QBOOT_HPATCH_USE_STORAGE_SWAP or QBOOT_HPATCH_USE_RAM_BUFFER."
#endif
    };

    hpi_patch_result_t result = HPATCHI_PATCH_ERROR;
    rt_bool_t swap_inited = RT_FALSE;

    if (_header_io_ops->ioctl(old_part, QBOOT_IO_CMD_GET_ERASE_ALIGN, &instance.old_part_sector_size) != RT_EOK)
    {
        LOG_E("Qboot: Get erase align size failed.");
        result = HPATCHI_OPTIONS_ERROR;
        goto hpatch_cleanup;
    }
#if defined(QBOOT_HPATCH_SWAP_STORE_FS)
    if (QBOOT_HPATCH_SWAP_FILE_SIZE < (int)instance.old_part_sector_size)
    {
        LOG_E("HPatchLite swap file too small: %u < erase align %u.", (rt_uint32_t)QBOOT_HPATCH_SWAP_FILE_SIZE, instance.old_part_sector_size);
        result = HPATCHI_OPTIONS_ERROR;
        goto hpatch_cleanup;
    }
#endif
    if (!qbt_swap_alloc_buf(&instance))
    {
        result = HPATCHI_PATCH_ERROR;
        goto hpatch_cleanup;
    }
    if (!instance.swap->init(&instance))
    {
        LOG_E("Failed to initialize swap backend.");
        result = HPATCHI_PATCH_ERROR;
        goto hpatch_cleanup;
    }
    swap_inited = RT_TRUE;
    result = hpi_patch(&instance.parent, QBOOT_HPATCH_PATCH_CACHE_SIZE, QBOOT_HPATCH_DECOMPRESS_CACHE_SIZE, _do_read_patch, _do_read_old, _do_write_new);

    if (result == HPATCHI_SUCCESS && instance.swap->write_pos > 0)
    {
        if (!qbt_swap_commit(&instance))
        {
            result = HPATCHI_PATCH_ERROR;
        }
    }
hpatch_cleanup:
    if (swap_inited)
    {
        instance.swap->deinit(&instance);
    }
    qbt_swap_free_buf(&instance);

    // --- Finalization: Tail Erase and Verification ---
    if (result == HPATCHI_SUCCESS)
    {
        rt_uint32_t part_len = 0;
        if (_header_io_ops->size(old_part, &part_len) == RT_EOK && newer_file_len < (int)part_len)
        {
            LOG_I("New firmware is smaller than '%s' partition. Erasing aligned tail data...", instance.old_name);
            if (!qbt_erase_aligned_range(&instance, newer_file_len, part_len, RT_TRUE))
            {
                result = HPATCHI_FILEWRITE_ERROR;
            }
        }
    }

    // Final check and result reporting
    if (result == HPATCHI_SUCCESS)
    {
        int final_len = 0;
        final_len = instance.committed_len;
        if (final_len == newer_file_len)
        {
            LOG_I("Update successful! Total size: %d bytes.", final_len);
        }
        else
        {
            LOG_E("Update finished, but final length (%d) != newer_file_len (%d)!", final_len, newer_file_len);
            result = HPATCHI_PATCH_ERROR;
        }
    }
    else
    {
        LOG_E("Update failed with error code: %d", result);
    }

    return (result == HPATCHI_SUCCESS);
}
/**
 * @brief  Placeholder decompressor for HPatchLite.
 * @note   This is a placeholder decompressor for HPatchLite.
 * @param  *buf: Input/output buffers.
 * @param  *out: Stream results.
 * @param  *ctx: Stream context.
 * @retval RT_EOK on success, error code otherwise.
 */
static rt_err_t qbt_algo_hpatchlite_decompress(const qbt_stream_buf_t *buf, qbt_stream_status_t *out, const qbt_stream_ctx_t *ctx)
{
    RT_UNUSED(ctx);
    if (out != RT_NULL)
    {
        out->consumed = 0;
        out->produced = 0;
        out->remaining_in = (buf != RT_NULL) ? buf->in_len : 0;
    }
    LOG_E("HPatchLite placeholder decompress called; use patch flow.");
    return -RT_ERROR;
}

static const qboot_cmprs_ops_t qbt_algo_hpatchlite_cmprs_ops = {
    .cmprs_name = "HPATCHLITE",
    .cmprs_id = QBOOT_ALGO_CMPRS_HPATCHLITE,
    .init = RT_NULL,
    .decompress = qbt_algo_hpatchlite_decompress,
    .deinit = RT_NULL,
};

/**
 * @brief Register HPatchLite compression ops.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
rt_err_t qbt_algo_hpatchlite_register(void)
{
    return qboot_cmprs_register(&qbt_algo_hpatchlite_cmprs_ops);
}
#endif // QBOOT_USING_HPATCHLITE
