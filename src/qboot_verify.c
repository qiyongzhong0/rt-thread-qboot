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

static tiny_md5_context ctx;

void qbt_verify_init(void)
{
    tiny_md5_starts(&ctx);
}

void qbt_verify_cal(const u8 *buf, u32 len)
{
    tiny_md5_update(&ctx, buf, len);
}

u32 qbt_verify_rst(void)
{
    u32 hash[4];
    u32 rst = 0;
    
    tiny_md5_finish(&ctx, (u8 *)hash);

    for(int i=0; i<sizeof(hash)/sizeof(u32); i++)
    {
        rst ^= hash[i];
    }

    return(rst);
}

#endif

