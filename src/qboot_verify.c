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

static tiny_sha1_context ctx;

void qbt_verify_init(void)
{
    tiny_sha1_starts(&ctx);
}

void qbt_verify_cal(const u8 *buf, u32 len)
{
    tiny_sha1_update(&ctx, buf, len);
}

u32 qbt_verify_rst(void)
{
    u32 hash[5];
    
    tiny_sha1_finish(&ctx, (u8 *)hash);

    return(hash[0]);
}

#endif

