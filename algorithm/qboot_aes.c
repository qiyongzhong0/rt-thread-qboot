/**
 * @file qboot_aes.c
 * @brief 
 * @author wdfk-prog ()
 * @version 1.0
 * @date 2026-01-16
 * 
 * @copyright Copyright (c) 2026  
 * 
 * @note :
 * @par Change Log:
 * Date       Version   Author      Description
 * 2020-07-06 1.0       qiyongzhong first version
 * 2026-01-16 1.1       wdfk-prog   add one-shot decompress handler for blocks
 */
#include <qboot.h>

#ifdef QBOOT_USING_AES

#include <tinycrypt.h>
#include <string.h>

static rt_uint8_t qbt_aes_iv[16];
static tiny_aes_context qbt_aes_ctx;

/**
 * @brief Initializes the AES decryption context.
 *
 * This function sets up the initialization vector (IV) and the decryption key
 * required for AES decryption operations. It copies a predefined IV and sets
 * a 256-bit AES decryption key.
 */
void qbt_aes_decrypt_init(void)
{
    int len = strlen(QBOOT_AES_IV);
    if (len > sizeof(qbt_aes_iv))
    {
        len = sizeof(qbt_aes_iv);
    }
    memset(qbt_aes_iv, 0, sizeof(qbt_aes_iv));
    memcpy(qbt_aes_iv, QBOOT_AES_IV, len);
    tiny_aes_setkey_dec(&qbt_aes_ctx, (rt_uint8_t *)QBOOT_AES_KEY, 256);
}

/**
 * @brief Decrypts a block of data using AES CBC mode.
 *
 * This function performs AES decryption in Cipher Block Chaining (CBC) mode on a
 * given source buffer and places the result in the destination buffer.
 *
 * @param[out] dst_buf  Pointer to the destination buffer for the decrypted data.
 * @param[in]  src_buf  Pointer to the source buffer containing the encrypted data.
 * @param[in]  len      The length, in bytes, of the data to be decrypted.
 */
void qbt_aes_decrypt(rt_uint8_t *dst_buf, const rt_uint8_t *src_buf, rt_uint32_t len)
{
    tiny_aes_crypt_cbc(&qbt_aes_ctx, AES_DECRYPT, len, qbt_aes_iv, (rt_uint8_t *)src_buf, dst_buf);    
}

/**
 * @brief Initialize AES crypto ops.
 *
 * @return RT_EOK on success.
 */
static rt_err_t qbt_algo_aes_init(void)
{
    qbt_aes_decrypt_init();
    return RT_EOK;
}

/**
 * @brief AES decrypt wrapper for crypto ops.
 *
 * @param out Output buffer for plaintext.
 * @param in  Input ciphertext buffer.
 * @param len Number of bytes to decrypt.
 *
 * @return RT_EOK on success.
 */
static rt_err_t qbt_algo_aes_crypt(rt_uint8_t *out, const rt_uint8_t *in, rt_uint32_t len)
{
    qbt_aes_decrypt(out, in, len);
    return RT_EOK;
}

/** AES crypto ops for Qboot. */
static const qboot_crypto_ops_t qbt_algo_aes_crypt_ops = {
    .crypto_name = "aes",
    .crypto_id = QBOOT_ALGO_CRYPT_AES,
    .init = qbt_algo_aes_init,
    .decrypt = qbt_algo_aes_crypt,
    .deinit = RT_NULL,
};

/**
 * @brief Register AES algorithm ops into Qboot.
 *
 * @return RT_EOK on success, negative error code on failure.
 */
rt_err_t qbt_algo_aes_register(void)
{
    return qboot_crypto_register(&qbt_algo_aes_crypt_ops);
}

#endif

