/**
 * @file qboot_stream.h
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
 * 2026-01-15 1.0     wdfk-prog   split stream interfaces
 */
#ifndef __QBOOT_STREAM_H__
#define __QBOOT_STREAM_H__

#include "qboot_ops.h"

struct qbt_algo_context;
typedef struct qbt_algo_context qbt_algo_context_t;

/**
 * @brief Purpose of a stream decompression pass.
 */
typedef enum
{
    QBT_STREAM_WRITE = 0,   /**< Produce output for writing to destination. */
    QBT_STREAM_CRC = 1,     /**< Produce output only for CRC calculation. */
} qbt_stream_purpose_t;

/**
 * @brief Stream input/output buffers for a decompress call.
 */
typedef struct
{
    rt_uint8_t *in;         /**< Input buffer pointer (in/out). */
    rt_uint32_t in_len;     /**< Input buffer length (in/out). */
    rt_uint8_t *out;        /**< Output buffer pointer. */
    rt_uint32_t out_len;    /**< Output buffer capacity. */
} qbt_stream_buf_t;

/**
 * @brief Stream call results.
 * @note:
 * - On RT_EOK, the implementation must make progress: consumed > 0 or produced > 0.
 * - When more input is required, return -RT_ENOSPC and set consumed = 0 and produced = 0.
 */
typedef struct
{
    rt_uint32_t consumed;       /**< Bytes consumed from input. */
    rt_uint32_t produced;       /**< Bytes produced to output. */
    rt_uint32_t remaining_in;   /**< Remaining input bytes after this call (0 when unknown). */
} qbt_stream_status_t;

/**
 * @brief Decompression stream context passed to each decompress call.
 */
typedef struct
{
    rt_uint32_t total;              /**< Total compressed length; 0 when unknown. */
    rt_uint32_t consumed;           /**< Compressed bytes consumed before this call. */
    rt_uint32_t raw_remaining;      /**< Raw output budget for this call (0 for unlimited). */
    qbt_stream_purpose_t purpose;   /**< Stream purpose (write/CRC). */
} qbt_stream_ctx_t;

/**
 * @brief Fixed configuration for a stream processing operation.
 */
typedef struct
{
    void *src_handle;                 /**< Package source handle. */
    void *dst_handle;                 /**< Destination handle (optional). */
    const fw_info_t *fw_info;         /**< Firmware info header. */
    const qbt_algo_context_t *algo_ops; /**< Algorithm handler table. */
    rt_uint8_t *cmprs_buf;            /**< Buffer to accumulate compressed input. */
    rt_uint8_t *out_buf;              /**< Buffer to hold decompressed output. */
    rt_uint8_t *crypt_buf;            /**< Scratch buffer for encrypted input. */
} qbt_stream_cfg_t;

/**
 * @brief Stream state used by the write path.
 *
 * Carries destination handle and current raw output offset while streaming.
 * The stream loop updates @ref raw_pos before each call, and the write consumer
 * advances it after successful writes.
 */
typedef struct
{
    void *dst_handle;       /**< Destination handle for writes (partition/file). */
    rt_uint32_t raw_pos;    /**< Current raw output offset (bytes). */
    rt_uint32_t raw_size;   /**< Total raw size for progress display (0 to disable). */
} qbt_stream_state_t;

/**
 * @brief Stream processor callback for decompression output.
 *
 * @param algo_ops    Algorithm ops providing decompress handler.
 * @param stream_buf  Stream IO buffer (in_len updated with remaining input).
 * @param cmprs_ctx   Stream context for this call.
 * @param out         [out] Stream results for this call.
 * @param ctx         User context provided by the caller.
 *
 * @return RT_EOK on success, -RT_ENOSPC when more input is needed, or negative error code.
 */
typedef rt_err_t (*qbt_stream_proc_t)(const qbt_algo_context_t *algo_ops, qbt_stream_buf_t *stream_buf, const qbt_stream_ctx_t *cmprs_ctx, qbt_stream_status_t *out, void *ctx);

rt_bool_t qbt_fw_stream_process(const qbt_stream_cfg_t *cfg, qbt_stream_purpose_t purpose, qbt_stream_proc_t proc, void *proc_ctx);
rt_err_t qbt_stream_crc_proc(const qbt_algo_context_t *algo_ops, qbt_stream_buf_t *stream_buf,
                             const qbt_stream_ctx_t *cmprs_ctx, qbt_stream_status_t *out, void *ctx);
rt_err_t qbt_stream_write_proc(const qbt_algo_context_t *algo_ops, qbt_stream_buf_t *stream_buf,
                               const qbt_stream_ctx_t *cmprs_ctx, qbt_stream_status_t *out, void *ctx);

#endif
