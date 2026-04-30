/**
 * @file qboot_quicklz.c
 * @brief QuickLZ decompression adapter for Qboot.
 * @author wdfk-prog ()
 * @version 1.0
 * @date 2026-01-13
 *
 * @copyright Copyright (c) 2026
 *
 * @note
 * - Provides one-shot block decompression for QuickLZ packaged data.
 * @par Change Log:
 * Date       Version   Author      Description
 * 2020-07-06 1.0       qiyongzhong first version
 * 2026-01-13 1.1       wdfk-prog   add one-shot decompress handler for QUICKLZ blocks
 */
#include <qboot.h>

#ifdef QBOOT_USING_QUICKLZ
#include <quicklz.h>

#define DBG_TAG "qb_quicklz"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#define QBOOT_QUICKLZ_BLOCK_HDR_SIZE    4

/** QuickLZ decompression state for streaming API. */
static qlz_state_decompress qbt_quicklz_state;

/**
 * @brief Initialize QuickLZ decompression state.
 */
void qbt_quicklz_state_init(void)
{
    rt_memset((rt_uint8_t *)&qbt_quicklz_state, 0, sizeof(qbt_quicklz_state));
}

/**
 * @brief Parse the compressed block size from the QuickLZ block header.
 *
 * @param comp_datas Input buffer containing the block header.
 *
 * @return Block size parsed from the header (big-endian).
 */
rt_uint32_t qbt_quicklz_get_block_size(const rt_uint8_t *comp_datas)
{
    rt_uint32_t block_size = 0;
    for (int i = 0; i < QBOOT_QUICKLZ_BLOCK_HDR_SIZE; i++)
    {
        block_size <<= 8;
        block_size += comp_datas[i];
    }
    return (block_size);
}

/**
 * @brief Decompress a QuickLZ block payload (no header).
 *
 * @param out_buf Output buffer for decompressed data.
 * @param in_buf  Input buffer pointing to the compressed payload.
 *
 * @return Decompressed length.
 */
rt_uint32_t qbt_quicklz_decompress(rt_uint8_t *out_buf, const rt_uint8_t *in_buf)
{
    return (qlz_decompress((char *)in_buf, out_buf, &qbt_quicklz_state));
}

/**
 * @brief One-shot decompress handler for QuickLZ blocks.
 *
 * @param buf Input/output buffers.
 * @param out [out] Stream results.
 * @param ctx Decompression stream context (unused for QuickLZ).
 *
 * @return RT_EOK on success, -RT_ENOSPC when more input is needed, or -RT_ERROR on failure.
 */
static rt_err_t qbt_algo_quicklz_decompress(const qbt_stream_buf_t *buf, qbt_stream_status_t *out, const qbt_stream_ctx_t *ctx)
{
    RT_UNUSED(ctx);
    rt_uint32_t block_size; /**< Compressed block size. */
    rt_uint32_t need_len;
    rt_uint32_t decomp_len;

    if (buf->in_len < QBOOT_QUICKLZ_BLOCK_HDR_SIZE)
    {
        return -RT_ENOSPC;
    }

    block_size = qbt_quicklz_get_block_size(buf->in);
    need_len = block_size + QBOOT_QUICKLZ_BLOCK_HDR_SIZE;
    if (block_size == 0)
    {
        return -RT_ERROR;
    }
    else if (buf->in_len < need_len)
    {
        return -RT_ENOSPC;
    }

    decomp_len = qbt_quicklz_decompress(buf->out, buf->in + QBOOT_QUICKLZ_BLOCK_HDR_SIZE);
    if (decomp_len == 0)
    {
        LOG_E("Qboot quicklz decompress error", decomp_len);
        return -RT_ERROR;
    }
    else if (decomp_len > buf->out_len)
    {
        LOG_W("Qboot quicklz decompress warn. decomp_len=%d > out_len=%u", decomp_len, (rt_uint32_t)buf->out_len);
        decomp_len = buf->out_len;
        return -RT_ERROR;
    }

    out->consumed = need_len;
    out->produced = (rt_uint32_t)decomp_len;
    return RT_EOK;
}

/**
 * @brief Initialize QuickLZ algorithm state for decompression.
 *
 * @return RT_EOK on success.
 */
static rt_err_t qbt_algo_quicklz_init(void)
{
    qbt_quicklz_state_init();
    return RT_EOK;
}

/** QuickLZ compression ops for Qboot. */
static const qboot_cmprs_ops_t qbt_algo_quicklz_cmprs_ops = {
    .cmprs_name = "quicklz",
    .cmprs_id = QBOOT_ALGO_CMPRS_QUICKLZ,
    .init = qbt_algo_quicklz_init,
    .decompress = qbt_algo_quicklz_decompress,
    .deinit = RT_NULL,
};

/**
 * @brief Register QuickLZ algorithm ops into Qboot.
 *
 * @return RT_EOK on success, negative error code on failure.
 */
rt_err_t qbt_algo_quicklz_register(void)
{
    return qboot_cmprs_register(&qbt_algo_quicklz_cmprs_ops);
}

#endif
