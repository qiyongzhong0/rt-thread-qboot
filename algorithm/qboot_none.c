/**
 * @file qboot_algo_none.c
 * @brief 
 * @author wdfk-prog ()
 * @version 1.0
 * @date 2026-01-12
 * 
 * @copyright Copyright (c) 2026  
 * 
 * @note :
 * @par Change Log:
 * Date       Version Author      Description
 * 2026-01-12 1.0     wdfk-prog   first version
 */
#include <qboot.h>

/**
 * @brief Pass-through decompress handler for NONE algorithm.
 *
 * Simply forwards input to output. When input and output buffers are the same,
 * no copy is performed.
 *
 * @param buf Input/output buffers.
 * @param ctx Decompression stream context (unused for NONE).
 * @param buf Input/output buffers.
 * @param out [out] Stream results.
 * @param ctx Decompression stream context.
 *
 * @return RT_EOK on success, -RT_ENOSPC when more input is needed, or -RT_ERROR on failure.
 */
static rt_err_t qbt_algo_none_decompress(const qbt_stream_buf_t *buf, qbt_stream_status_t *out, const qbt_stream_ctx_t *ctx)
{
    RT_UNUSED(ctx);
    if (buf->in_len == 0)
    {
        return -RT_ENOSPC;
    }
#ifdef QBOOT_USING_COMPRESSION
    rt_uint32_t copy_len = (buf->in_len < buf->out_len) ? buf->in_len : buf->out_len;
    if (copy_len > 0)
    {
        rt_memcpy(buf->out, buf->in, copy_len);
    }
    out->consumed = copy_len;
    out->produced = copy_len;
#else
    if (buf->in == buf->out)
    {
        out->consumed = buf->in_len;
        out->produced = buf->in_len;
    }
    else
    {
        return -RT_ERROR;
    }
#endif /* QBOOT_USING_COMPRESSION */
    return RT_EOK;
}

/**
 * @brief      A placeholder algorithm that copies data without modification.
 *
 *             This function is used when no encryption or other processing is required.
 *             It copies the data from the input buffer to the output buffer.
 *
 * @note       When `QBOOT_USING_COMPRESSION` is not defined, the copy operation is
 *             bypassed if the input and output buffers are different. When it is
 *             defined, a direct memory copy is always performed.
 *
 * @param[out] out   Pointer to the destination buffer.
 * @param[in]  in    Pointer to the source buffer.
 * @param[in]  len   The number of bytes to copy.
 *
 * @return     Always returns RT_EOK to indicate success.
 */
rt_err_t qbt_algo_none_crypt(rt_uint8_t *out, const rt_uint8_t *in, rt_uint32_t len)
{
#ifdef QBOOT_USING_COMPRESSION
    rt_memcpy(out, in, len);
#endif /* QBOOT_USING_COMPRESSION */
    return RT_EOK;
}

static const qboot_cmprs_ops_t qbt_algo_none_cmprs_ops = {
    .cmprs_name = "NONE",
    .cmprs_id = QBOOT_ALGO_CMPRS_NONE,
    .init = RT_NULL,
    .decompress = qbt_algo_none_decompress,
    .deinit = RT_NULL,
};

/** AES crypto ops for Qboot. */
static const qboot_crypto_ops_t qbt_algo_none_crypt_ops = {
    .crypto_name = "NONE",
    .crypto_id = QBOOT_ALGO_CRYPT_NONE,
    .init = RT_NULL,
    .decrypt = qbt_algo_none_crypt,
    .deinit = RT_NULL,
};

/**
 * @brief Register the NONE algorithm entry during initialization.
 *
 * @return RT_EOK if the registration succeeds, or -RT_ERROR on failure.
 */
rt_err_t qbt_algo_none_register(void)
{
    if(qboot_cmprs_register(&qbt_algo_none_cmprs_ops) != RT_EOK)
        return -RT_ERROR;
    if(qboot_crypto_register(&qbt_algo_none_crypt_ops) != RT_EOK)
        return -RT_ERROR;
    return RT_EOK;
}
