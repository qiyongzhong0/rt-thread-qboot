/**
 * @file qboot_algo.c
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
 * 2026-01-15 1.0     wdfk-prog   split ops helpers from qboot.c
 */
#include <qboot_algo.h>

#define DBG_TAG "qb_algo"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

static const qboot_cmprs_ops_t  *g_cmprs_table[QBOOT_ALGO_CMPRS_COUNT] = {0};
static const qboot_crypto_ops_t *g_crypt_table[QBOOT_ALGO_CRYPT_COUNT] = {0};

/**
 * @brief      A macro to simplify the process of registering an algorithm.
 *
 *             It executes a function call, checks its return value, and logs an
 *             error if the registration fails.
 *
 * @param[in]  fn_call  The algorithm registration function to call.
 */
#define QBT_REGISTER_ALGO(fn_call)                                \
    do                                                            \
    {                                                             \
        if ((fn_call) != RT_EOK)                                  \
        {                                                         \
            LOG_E("Qboot register algorithm fail: %s", #fn_call); \
            return -RT_ERROR;                                     \
        }                                                         \
    } while (0)

/**
 * @brief      Converts a compression algorithm ID to its corresponding index in the table.
 *
 * @param[in]  algo_id  The algorithm identifier.
 *
 * @return     The calculated index if valid, or QBOOT_ALGO_CMPRS_COUNT if invalid.
 */
static rt_uint32_t qboot_cmprs_id_to_index(rt_uint16_t algo_id)
{
    rt_uint16_t cmprs_idx = (algo_id >> 8);
    if (cmprs_idx >= QBOOT_ALGO_CMPRS_COUNT)
    {
        return QBOOT_ALGO_CMPRS_COUNT;
    }
    return cmprs_idx;
}

/**
 * @brief      Converts a crypto algorithm ID to its corresponding index in the table.
 *
 * @param[in]  algo_id  The algorithm identifier.
 *
 * @return     The calculated index if valid, or QBOOT_ALGO_CRYPT_COUNT if invalid.
 */
static rt_uint32_t qboot_crypto_id_to_index(rt_uint16_t algo_id)
{
    rt_uint16_t crypt_id = algo_id & QBOOT_ALGO_CRYPT_MASK;
    if (crypt_id >= QBOOT_ALGO_CRYPT_COUNT)
    {
        return QBOOT_ALGO_CRYPT_COUNT;
    }

    return crypt_id;
}

/**
 * @brief      Registers a set of compression operations.
 *
 *             This function validates the provided operations structure and adds it to the
 *             global compression table if the corresponding slot is available.
 *
 * @param[in]  ops   A pointer to the compression operations structure to register.
 *
 * @return     RT_EOK on success, -RT_ERROR on failure (e.g., null pointers,
 *             invalid ID, or already registered).
 */
rt_err_t qboot_cmprs_register(const qboot_cmprs_ops_t *ops)
{
    if ((ops == RT_NULL) || (ops->decompress == RT_NULL) || (ops->cmprs_name == RT_NULL))
    {
        return -RT_ERROR;
    }

    rt_uint32_t idx = qboot_cmprs_id_to_index(ops->cmprs_id);
    if (idx >= QBOOT_ALGO_CMPRS_COUNT || g_cmprs_table[idx] != RT_NULL)
    {
        return -RT_ERROR;
    }

    g_cmprs_table[idx] = ops;
    return RT_EOK;
}

/**
 * @brief      Registers a set of cryptographic operations.
 *
 *             This function validates the provided operations structure and adds it to the
 *             global crypto table if the corresponding slot is available.
 *
 * @param[in]  ops   A pointer to the crypto operations structure to register.
 *
 * @return     RT_EOK on success, -RT_ERROR on failure (e.g., null pointers,
 *             invalid ID, or already registered).
 */
rt_err_t qboot_crypto_register(const qboot_crypto_ops_t *ops)
{
    if ((ops == RT_NULL) || (ops->decrypt == RT_NULL) || (ops->crypto_name == RT_NULL))
    {
        return -RT_ERROR;
    }

    rt_uint32_t idx = qboot_crypto_id_to_index(ops->crypto_id);
    if (idx >= QBOOT_ALGO_CRYPT_COUNT || g_crypt_table[idx] != RT_NULL)
    {
        return -RT_ERROR;
    }

    g_crypt_table[idx] = ops;
    return RT_EOK;
}

/**
 * @brief      Finds a registered cryptographic algorithm by its ID.
 *
 * @param[in]  id    The ID of the crypto algorithm to find.
 *
 * @return     A pointer to the corresponding crypto operations structure if found,
 *             otherwise RT_NULL.
 */
static const qboot_crypto_ops_t *qboot_find_crypto(rt_uint16_t id)
{
    rt_uint32_t idx = qboot_crypto_id_to_index(id);
    if (idx >= QBOOT_ALGO_CRYPT_COUNT) return RT_NULL;
    return g_crypt_table[id];
}

/**
 * @brief      Finds a registered compression algorithm by its ID.
 *
 * @param[in]  id    The ID of the compression algorithm to find.
 *
 * @return     A pointer to the corresponding compression operations structure if found,
 *             otherwise RT_NULL.
 */
static const qboot_cmprs_ops_t *qboot_find_cmprs(rt_uint16_t id)
{
    rt_uint32_t idx = qboot_cmprs_id_to_index(id);
    if (idx >= QBOOT_ALGO_CMPRS_COUNT) return RT_NULL;
    return g_cmprs_table[idx];
}

/**
 * @brief      Initializes the algorithms specified in the context.
 *
 *             This function calls the init function for both the cryptographic and
 *             compression operations if they are defined in the context. It handles
 *             cleanup of the crypto module if the compression module fails to initialize.
 *
 * @param[in]  ctx   A pointer to the algorithm context.
 *
 * @return     RT_TRUE on successful initialization, RT_FALSE on failure.
 */
rt_bool_t qbt_fw_algo_init(const qbt_algo_context_t *ctx)
{
    if (ctx->crypt_ops && ctx->crypt_ops->init)
    {
        if (ctx->crypt_ops->init() != RT_EOK)
        {
            LOG_E("Crypto %s init fail", ctx->crypt_ops->crypto_name);
            return RT_FALSE;
        }
    }

    if (ctx->cmprs_ops && ctx->cmprs_ops->init)
    {
        if (ctx->cmprs_ops->init() != RT_EOK)
        {
            LOG_E("Cmprs %s init fail", ctx->cmprs_ops->cmprs_name);
            if (ctx->crypt_ops && ctx->crypt_ops->deinit)
                ctx->crypt_ops->deinit();
            return RT_FALSE;
        }
    }
    return RT_TRUE;
}

/**
 * @brief      De-initializes the algorithms specified in the context.
 *
 *             This function calls the deinit function for both the compression and
 *             cryptographic operations if they are defined in the context.
 *
 * @param[in]  ctx   A pointer to the algorithm context.
 */
void qbt_fw_algo_deinit(const qbt_algo_context_t *ctx)
{
    if (ctx->cmprs_ops && ctx->cmprs_ops->deinit)
        ctx->cmprs_ops->deinit();

    if (ctx->crypt_ops && ctx->crypt_ops->deinit)
        ctx->crypt_ops->deinit();
}

/**
 * @brief      Parses the firmware header and populates the algorithm context.
 *
 *             This function reads the algorithm identifier from the firmware information,
 *             finds the corresponding registered crypto and compression operations,
 *             and fills the context structure with pointers to them.
 *
 * @param[in]  fw_info  Pointer to the firmware information structure.
 * @param[out] ctx      Pointer to the algorithm context to be populated.
 *
 * @return     RT_TRUE if the required operations are found and the context is
 *             successfully populated, RT_FALSE otherwise.
 */
rt_bool_t qbt_fw_get_algo_context(const fw_info_t *fw_info, qbt_algo_context_t *ctx)
{
    /* 1. 分离 ID */
    rt_uint16_t crypt_id = fw_info->algo & QBOOT_ALGO_CRYPT_MASK;
    rt_uint16_t cmprs_id = fw_info->algo & QBOOT_ALGO_CMPRS_MASK;

    /* 2. 分别查找 */
    ctx->crypt_ops = qboot_find_crypto(crypt_id);
    ctx->cmprs_ops = qboot_find_cmprs(cmprs_id);

    if (ctx->cmprs_ops == RT_NULL || ctx->crypt_ops == RT_NULL)
    {
        return RT_FALSE;
    }
    else
    {
        return RT_TRUE;
    }
}

/**
 * @brief Register built-in algorithms based on build-time options.
 *
 * @return RT_EOK on success, negative error code on failure.
 */
rt_err_t qbot_algo_startup(void)
{
#ifdef QBOOT_ALGO_CRYPT_NONE
    extern rt_err_t qbt_algo_none_register(void);
    QBT_REGISTER_ALGO(qbt_algo_none_register());
#endif // QBOOT_ALGO_CRYPT_NONE
#ifdef QBOOT_USING_GZIP
    extern rt_err_t qbt_algo_gzip_register(void);
    QBT_REGISTER_ALGO(qbt_algo_gzip_register());
#endif // QBOOT_USING_GZIP
#ifdef QBOOT_USING_AES
    extern rt_err_t qbt_algo_aes_register(void);
    QBT_REGISTER_ALGO(qbt_algo_aes_register());
#endif // QBOOT_USING_AES
#ifdef QBOOT_USING_QUICKLZ
    extern rt_err_t qbt_algo_quicklz_register(void);
    QBT_REGISTER_ALGO(qbt_algo_quicklz_register());
#endif // QBOOT_USING_QUICKLZ
#ifdef QBOOT_USING_FASTLZ
    extern rt_err_t qbt_algo_fastlz_register(void);
    QBT_REGISTER_ALGO(qbt_algo_fastlz_register());
#endif // QBOOT_USING_FASTLZ
#ifdef QBOOT_USING_HPATCHLITE
    extern rt_err_t qbt_algo_hpatchlite_register(void);
    QBT_REGISTER_ALGO(qbt_algo_hpatchlite_register());
#endif // QBOOT_USING_HPATCHLITE
    return RT_EOK;
}
