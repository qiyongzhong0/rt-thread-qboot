/*
 * qboot_verify.c
 *
 * Change Logs:
 * Date           Author            Notes
 * 2020-07-10     qiyongzhong       first version
 */

#include <qboot_verify.h>

#ifdef QBOOT_USING_VERIFY

#include <tinycrypt.h>

static u8 qbt_aes_iv[16];
static tiny_aes_context qbt_aes_ctx;

void qbt_verify_init(void)
{
    
}

void qbt_verify_cal(const u8 *buf, u32 len)
{

}

bool qbt_verify_rst(u8 *buf, u32 buf_size)
{

}
#endif

