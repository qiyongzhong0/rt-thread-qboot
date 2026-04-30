/**
 * @file qboot_stream.c
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
 * 2026-01-15 1.0     wdfk-prog   split stream helpers from qboot.c
 */
#include <qboot.h>

#define DBG_TAG "qb_stream"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

/**
 * @brief Read a package fragment and optionally decrypt into caller buffer.
 *
 * @param src_handle  Source handle (partition/file) to read from.
 * @param src_off     Byte offset within the source.
 * @param out_buf     Output buffer for plaintext.
 * @param crypt_buf   Scratch buffer for cipher text (may alias @p out_buf when no decrypt).
 * @param read_len    Number of bytes to read.
 * @param algo_ops    Algorithm ops describing decrypt handler; may be NULL for raw.
 *
 * @return RT_TRUE on success, RT_FALSE on read/decrypt failure.
 */
rt_bool_t qbt_fw_pkg_read(void *src_handle, rt_uint32_t src_off, rt_uint8_t *out_buf, rt_uint8_t *crypt_buf, rt_uint32_t read_len, const qbt_algo_context_t *algo_ops)
{
    if (algo_ops->crypt_ops != RT_NULL)
    {
        if (_header_io_ops->read(src_handle, src_off, crypt_buf, read_len) != RT_EOK)
        {
            return (RT_FALSE);
        }

        return (algo_ops->crypt_ops->decrypt(out_buf, crypt_buf, read_len) == RT_EOK);
    }
    else
    {
        return (_header_io_ops->read(src_handle, src_off, out_buf, read_len) == RT_EOK);
    }
}

/**
 * @brief Consumer callback for a decompressed output chunk.
 *
 * @param buf Output data buffer.
 * @param len Output data length.
 * @param ctx User context provided by the caller.
 *
 * @return RT_EOK on success, negative error code otherwise.
 */
typedef rt_err_t (*qbt_cmprs_consumer_t)(const rt_uint8_t *buf, rt_uint32_t len, void *ctx);

/**
 * @brief Decompress compressed input and forward each produced chunk to a consumer.
 *
 * @param algo_ops     Algorithm ops providing decompress handler.
 * @param stream_buf   Stream IO buffer; @p in_len is updated with remaining input.
 * @param cmprs_ctx    Stream context for this call (may be NULL).
 * @param out          [out] Stream results for this call.
 * @param consumer     Callback invoked for each produced chunk.
 * @param consumer_ctx User data passed to @p consumer.
 *
 * @return RT_EOK on success, -RT_ENOSPC when more input is needed, or negative error code.
 */
static rt_err_t qbt_decompress_with_consumer(const qbt_algo_context_t *algo_ops, qbt_stream_buf_t *stream_buf,
                                             const qbt_stream_ctx_t *cmprs_ctx, qbt_stream_status_t *out,
                                             qbt_cmprs_consumer_t consumer, void *consumer_ctx)
{
    rt_uint32_t cmprs_len = stream_buf->in_len;
    rt_uint32_t remaining_out = cmprs_ctx->raw_remaining;
    qbt_stream_status_t step = { 0 };
    rt_err_t rst = RT_EOK;

    /* Keep looping while output budget remains. */
    while (remaining_out > 0)
    {
        /* (1) Build per-iteration context and decide empty-input flush. */
        rt_uint32_t cur_cap = (remaining_out < stream_buf->out_len) ? remaining_out : stream_buf->out_len; /**< Output cap for this iteration. */
        qbt_stream_ctx_t call_ctx = *cmprs_ctx; /**< Local context copy. */
        call_ctx.raw_remaining = remaining_out; /**< Refresh remaining raw budget. */
        call_ctx.consumed = cmprs_ctx->consumed + out->consumed; /**< Update consumed offset. */

        if (cmprs_len == 0) /**< No buffered compressed input. */
        {
            if (call_ctx.consumed < call_ctx.total) /**< Stream not ended. */
            {
                break;  /**< Wait for more input. */
            }
            else
            {
                /**< Allow a single flush with empty input. */
            }
        }

        /* (2) Invoke decompressor and validate progress paths. */
        qbt_stream_buf_t io = { stream_buf->in, (rt_uint32_t)cmprs_len, stream_buf->out, cur_cap };
        rst = algo_ops->cmprs_ops->decompress(&io, &step, &call_ctx);
        if (rst == -RT_ENOSPC) /**< Need more input data. */
        {
            break;
        }
        else if ((rst != RT_EOK)                  /**< Decompress failed. */
                 || (step.consumed > cmprs_len)   /**< Consumed more input than available. */
                 || (step.produced > cur_cap))    /**< Produced more output than buffer capacity. */
        {
            break;
        }
        else if ((step.consumed == 0) && (step.produced == 0))
        {
            break; /**< keep a minimal progress guard to avoid infinite loops */
        }

        /* (3) Dispatch output and update counters/budget. */
        rst = consumer(stream_buf->out, step.produced, consumer_ctx);
        if (rst != RT_EOK)
        {
            break;
        }

        /* Accumulate output and enforce the optional total output cap. */
        remaining_out -= step.produced;
        out->consumed += step.consumed;
        out->produced += step.produced;

        /* (4) Compact input buffer and track empty flush usage. */
        cmprs_len -= step.consumed;
        if (cmprs_len > 0)
        {
            rt_memmove(stream_buf->in, stream_buf->in + step.consumed, cmprs_len); /**< Move remaining input forward. */
        }
    }

    /* Report remaining compressed data to the caller. */
    out->remaining_in = stream_buf->in_len = (rt_uint32_t)cmprs_len;
    return rst;
}

/**
 * @brief Stream package data, decompress, and consume output with strict raw-size limits.
 *
 * This function loops over the package content, reads compressed data into a staging
 * buffer, calls the decompressor, and forwards produced data to @p proc. Each call
 * enforces a maximum output length equal to the remaining raw size to avoid overruns.
 */
rt_bool_t qbt_fw_stream_process(const qbt_stream_cfg_t *cfg, qbt_stream_purpose_t purpose,
                                qbt_stream_proc_t proc, void *proc_ctx)
{
    /* Track package read position, raw output position, and buffered input length. */
    rt_uint32_t src_read_pos =  qboot_src_read_pos();
    rt_uint32_t pkg_size = cfg->fw_info->pkg_size + qboot_src_read_pos();
    rt_uint32_t raw_pos = 0;
    rt_uint32_t cmprs_len = 0;

    /* Continue until the expected raw size has been produced. */
    while (raw_pos < cfg->fw_info->raw_size)
    {
        /* Limit read length to remaining package bytes. */
        int remain_len = pkg_size - src_read_pos;
        if (remain_len > 0)
        {
            rt_uint32_t read_len = QBOOT_CMPRS_READ_SIZE;
            if (read_len > remain_len)
            {
                read_len = remain_len;
            }

            /* Read (and decrypt if required) into the tail of the compressed buffer. */
            if (!qbt_fw_pkg_read(cfg->src_handle, src_read_pos, cfg->cmprs_buf + cmprs_len, cfg->crypt_buf, read_len, cfg->algo_ops))
            {
                LOG_E("Qboot stream read pkg error. addr=%08X, len=%d", src_read_pos, read_len);
                return RT_FALSE;
            }

            /* Advance read position and update buffered compressed length. */
            src_read_pos += read_len;
            cmprs_len += (rt_uint32_t)read_len;
        }
        else if (remain_len < 0)
        {
            LOG_E("Qboot stream read pkg error. read beyond pkg size %d.", remain_len);
            return RT_FALSE;
        }
        else if (cmprs_len == 0)
        {
            LOG_E("Qboot stream read pkg error. no compressed input left.");
            return RT_FALSE;
        }

        /* Enforce strict raw output cap based on remaining expected size. */
        qbt_stream_ctx_t cmprs_ctx = {
            .total = cfg->fw_info->pkg_size,
            .consumed = (rt_uint32_t)(src_read_pos - qboot_src_read_pos()) - cmprs_len,
            .raw_remaining = cfg->fw_info->raw_size - raw_pos,
            .purpose = purpose,
        };
        qbt_stream_buf_t stream_buf = { cfg->cmprs_buf, cmprs_len, cfg->out_buf, QBOOT_BUF_SIZE };
        qbt_stream_status_t out = { 0 };

        if (purpose == QBT_STREAM_WRITE)
        {
            ((qbt_stream_state_t *)proc_ctx)->raw_pos = raw_pos;
        }
        rt_err_t rst = proc(cfg->algo_ops, &stream_buf, &cmprs_ctx, &out, proc_ctx);
        if ((rst != RT_EOK && rst != -ENOSPC) || out.produced <= 0)
        {
            LOG_E("Qboot stream process error %d. addr=0X%08X, out_len = %d", rst, raw_pos, out.produced);
            return RT_FALSE;
        }
        cmprs_len = stream_buf.in_len;

        /* Advance raw output position. */
        raw_pos += (rt_uint32_t)out.produced;
    }

    return RT_TRUE;
}

#ifdef QBOOT_USING_APP_CHECK
/**
 * @brief CRC consumer for a decompressed output chunk.
 *
 * @param buf Output data buffer.
 * @param len Output data length.
 * @param ctx CRC accumulator pointer (rt_uint32_t *).
 *
 * @return RT_EOK on success.
 */
static rt_err_t qbt_crc_chunk_consumer(const rt_uint8_t *buf, rt_uint32_t len, void *ctx)
{
    rt_uint32_t *crc_acc = (rt_uint32_t *)ctx;
    *crc_acc = crc32_cyc_cal(*crc_acc, (rt_uint8_t *)buf, len);
    return RT_EOK;
}

/**
 * @brief Stream processor that updates CRC from decompressed output.
 *
 * @param algo_ops    Algorithm ops providing decompress handler.
 * @param stream_buf  Stream IO buffer (input/output and capacities).
 * @param cmprs_ctx   Stream context for this call.
 * @param out         [out] Stream results for this call.
 * @param ctx         CRC accumulator pointer (rt_uint32_t *).
 *
 * @return RT_EOK on success, -RT_ENOSPC when more input is needed, or negative error code.
 */
rt_err_t qbt_stream_crc_proc(const qbt_algo_context_t *algo_ops, qbt_stream_buf_t *stream_buf,
                             const qbt_stream_ctx_t *cmprs_ctx, qbt_stream_status_t *out, void *ctx)
{
    rt_uint32_t *crc_acc = (rt_uint32_t *)ctx;
    return qbt_decompress_with_consumer(algo_ops, stream_buf, cmprs_ctx, out, qbt_crc_chunk_consumer, crc_acc);
}
#endif

/**
 * @brief Output consumer that writes produced data to destination storage.
 *
 * @param buf Output data buffer.
 * @param len Output data length.
 * @param ctx Stream state pointer (qbt_stream_state_t *).
 *
 * @return RT_EOK on success, negative error code on failure.
 */
static rt_err_t qbt_write_chunk_consumer(const rt_uint8_t *buf, rt_uint32_t len, void *ctx)
{
    qbt_stream_state_t *state = (qbt_stream_state_t *)ctx;
    if (_header_io_ops->write(state->dst_handle, state->raw_pos, buf, len) != RT_EOK)
    {
        return -RT_ERROR;
    }
    state->raw_pos += (rt_uint32_t)len;
    return RT_EOK;
}

/**
 * @brief Stream processor that writes decompressed output to destination storage.
 *
 * @param algo_ops    Algorithm ops providing decompress handler.
 * @param stream_buf  Stream IO buffer (input/output and capacities).
 * @param cmprs_ctx   Stream context for this call.
 * @param out         [out] Stream results for this call.
 * @param ctx         Stream state pointer (qbt_stream_state_t *).
 *
 * @return RT_EOK on success, -RT_ENOSPC when more input is needed, or negative error code.
 */
rt_err_t qbt_stream_write_proc(const qbt_algo_context_t *algo_ops, qbt_stream_buf_t *stream_buf,
                               const qbt_stream_ctx_t *cmprs_ctx, qbt_stream_status_t *out, void *ctx)
{
    qbt_stream_state_t *state = (qbt_stream_state_t *)ctx;
    rt_err_t rst = qbt_decompress_with_consumer(algo_ops, stream_buf, cmprs_ctx, out, qbt_write_chunk_consumer, state);
    if ((rst == RT_EOK) && (out->produced > 0) && (state->raw_size > 0))
    {
        rt_uint32_t percent = state->raw_pos * 100 / state->raw_size;
        rt_kprintf("\b\b\b%02d%%", percent);
        if (percent >= 100)
        {
            rt_kprintf("\n");
        }
    }
    return rst;
}
