from building import *

cwd = GetCurrentDir()
path = [cwd+'/inc']
src = [
    'src/qboot.c',
    'src/qboot_algo.c',
    'src/qboot_custom_ops.c',
    'src/qboot_fal_ops.c',
    'src/qboot_fs_ops.c',
    'src/qboot_mux_ops.c',
    'src/qboot_ops.c',
    'src/qboot_stream.c',
]
if GetDepend('QBOOT_USING_UPDATE_MGR'):
    src += ['src/qboot_update.c']

src += Glob('platform/*.c')
src += ['algorithm/qboot_none.c']

if GetDepend('QBOOT_USING_AES'):
    src += ['algorithm/qboot_aes.c']
if GetDepend('QBOOT_USING_GZIP'):
    src += ['algorithm/qboot_gzip.c']
if GetDepend('QBOOT_USING_QUICKLZ'):
    src += ['algorithm/qboot_quicklz.c']
if GetDepend('QBOOT_USING_FASTLZ'):
    src += ['algorithm/qboot_fastlz.c']
if GetDepend('QBOOT_USING_HPATCHLITE'):
    src += ['algorithm/qboot_hpatchlite.c']
 
group = DefineGroup('qboot', src, depend = ['PKG_USING_QBOOT'], CPPPATH = path)

Return('group')
