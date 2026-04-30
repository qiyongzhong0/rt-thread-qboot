# Quick bootloader

## 1. 简介

**Quick bootloader** 是一个用于快速制作bootloader的组件。

### 1.1 目录结构

`Quick bootloader` 软件包目录结构如下所示：

```
qboot
├───doc                                 // 说明文档目录
│   │   qboot各项配置资源占用情况说明.md  // 资源占用说明
│   │   qboot工作流程说明.md             // 工作流程说明
│   │   qboot命令详述.md                 // shell命令说明
│   │   qboot使用指导.md                 // 使用示例详解
│   │   qboot_update.md                 // update 模块说明
│   │   极简版Bootloader制作.md          // 极简bootloader制作示例
│   └───qboot状态指示灯说明.md           // 状态指示灯说明
├───inc                                 // 头文件目录
│   │   qboot.h                         // 主模块头文件
│   ├── qboot_algo.h                    // 算法模块头文件
│   ├── qboot_cfg.h                     // 配置模块头文件
│   ├── qboot_ops.h                     // 操作模块头文件
│   ├── qboot_stream.h                  // 流处理模块头文件
│   └── qboot_update.h                  // update 模块头文件
├───src                                 // 源码目录
│   │   qboot.c                         // 主模块
│   ├── qboot_algo.c                    // 算法模块
│   ├── qboot_custom_ops.c              // 自定义模块
│   ├── qboot_fal_ops.c                 // fal模块
│   ├── qboot_fs_ops.c                  // 文件系统模块
│   ├── qboot_mux_ops.c                 // 多路复用模块
│   ├── qboot_ops.c                     // 操作模块
│   ├── qboot_stream.c                  // 流处理模块
│   └── qboot_update.c                  // update 模块
├── algorithm                           // 算法模块
│   ├── qboot_none.c                    // 无算法模块
│   │   qboot_aes.c                     // aes解密模块
│   │   qboot_fastlz.c                  // fastlz解压模块
│   │   qboot_gzip.c                    // gzip解压模块
│   │   qboot_hpatchlite.c              // hpatchlite解压模块
│   └───qboot_quicklz                   // quicklz解压模块
├── platform                            // 平台模块
│   ├── qboot_at32.c                    // at32模块
│   ├── qboot_gd32.c                    // gd32模块
│   ├── qboot_hc32f460.c                // hc32f460模块
│   ├── qboot_n32.c                     // n32模块
│   └── qboot_stm32.c                   // stm32模块
├───tools                               // 工具目录
│   ├── package_tool.py                 // 升级包帧头打包工具
│   └───QBootPackager_V1.00.zip         // 升级包打包器
│   license                             // 软件包许可证
│   readme.md                           // 软件包使用说明
└───SConscript                          // RT-Thread 默认的构建脚本
```

### 1.2 许可证

Quick bootloader 遵循 LGPLv2.1 许可，详见 `LICENSE` 文件。

### 1.3 依赖

- RT_Thread 4.0
- fal
- crclib

### 1.4 设计实现与流程小结

- 启动时注册存储与算法：`qboot_register_storage_ops()` 绑定 `_header_io_ops/_header_parser_ops`，`qbot_algo_startup()` 注册加密/压缩算法。
- 目标角色与存储分离：`QBT_TARGET_LIST` 生成 `qbt_target_id_t` 与角色名映射；`fw_info.part_name` 仅和角色名比较，实际打开的存储由 `g_descs.store_name` 决定。
- 固件处理流程：读取 `fw_info` -> 校验包体/签名 -> 选择算法上下文。
- 常规包释放：走 `qbt_fw_stream_process()` 完成解密+解压+写入/CRC。
- HPatchLite 包：命中 `QBOOT_ALGO_CMPRS_HPATCHLITE` 时走差分流程；RAM/FLASH 统一用 swap 后端，`ram_buf` 在主流程统一分配（RAM 缓冲或 flash-to-flash copy buffer）。
- 完成后写回尾部头信息，并进行可选校验与标记。
- update 模块：`qboot_update.c` 负责升级原因、等待窗口、下载超时与下载数据统计，提供 download helper 统一处理 sign/擦除/写入流程，详见 [qboot_update 说明](doc/qboot_update.md)。

## 2. 使用

### 2.1 获取组件

- **方式1：**
通过 *Env配置工具* 或 *RT-Thread studio* 开启软件包，根据需要配置各项参数；配置路径为 *RT-Thread online packages -> system -> qboot* 

### 2.2 Kconfig 配置与文件

- 配置文件：`bl_lib/qboot/Kconfig`
- 入口：`bl_lib/Kconfig` 通过 `rsource "qboot/Kconfig"` 引入
- 关键项（节选）：
  - QBOOT_USING_UPDATE_MGR：启用 update 管理器
  - QBOOT_POLL_DELAY_MS：update 轮询延时（ms）
  - QBT_UPDATE_MGR_PROGRESS_ENABLE：下载进度输出
  - QBOOT_UPDATE_MGR_USE_DOWNLOAD_HELPER：download helper（sign/擦除/写入）

### 2.3 功能选项宏定义说明

| 选项宏 | 说明 |
| ---- | ---- |
| QBOOT_PKG_SOURCE_FAL | 启用 FAL 后端 |
| QBOOT_PKG_SOURCE_FS | 启用文件系统后端（实验） |
| QBOOT_PKG_SOURCE_CUSTOM | 启用自定义 flash 后端 |
| QBOOT_APP_STORE_FAL | APP 使用 FAL 分区 |
| QBOOT_APP_STORE_FS | APP 使用文件系统 |
| QBOOT_APP_STORE_CUSTOM | APP 使用自定义 flash |
| QBOOT_DOWNLOAD_STORE_FAL | DOWNLOAD 使用 FAL 分区 |
| QBOOT_DOWNLOAD_STORE_FS | DOWNLOAD 使用文件系统 |
| QBOOT_DOWNLOAD_STORE_CUSTOM | DOWNLOAD 使用自定义 flash |
| QBOOT_FACTORY_STORE_FAL | FACTORY 使用 FAL 分区 |
| QBOOT_FACTORY_STORE_FS | FACTORY 使用文件系统 |
| QBOOT_FACTORY_STORE_CUSTOM | FACTORY 使用自定义 flash |
| QBOOT_APP_PART_NAME | APP 角色名（fw_info.part_name 匹配与日志） |
| QBOOT_DOWNLOAD_PART_NAME | DOWNLOAD 角色名（fw_info.part_name 匹配与日志） |
| QBOOT_FACTORY_PART_NAME | FACTORY 角色名（fw_info.part_name 匹配与日志） |
| QBOOT_APP_FAL_PART_NAME | APP 使用的 FAL 分区名 |
| QBOOT_DOWNLOAD_FAL_PART_NAME | DOWNLOAD 使用的 FAL 分区名 |
| QBOOT_FACTORY_FAL_PART_NAME | FACTORY 使用的 FAL 分区名 |
| QBOOT_APP_FILE_PATH | APP 文件路径（FS） |
| QBOOT_APP_SIGN_FILE_PATH | APP sign 文件路径（FS） |
| QBOOT_DOWNLOAD_FILE_PATH | DOWNLOAD 文件路径（FS） |
| QBOOT_DOWNLOAD_SIGN_FILE_PATH | DOWNLOAD sign 文件路径（FS） |
| QBOOT_FACTORY_FILE_PATH | FACTORY 文件路径（FS） |
| QBOOT_APP_FLASH_ADDR | APP 自定义 flash 基地址 |
| QBOOT_APP_FLASH_LEN | APP 自定义 flash 长度 |
| QBOOT_DOWNLOAD_FLASH_ADDR | DOWNLOAD 自定义 flash 基地址 |
| QBOOT_DOWNLOAD_FLASH_LEN | DOWNLOAD 自定义 flash 长度 |
| QBOOT_FACTORY_FLASH_ADDR | FACTORY 自定义 flash 基地址 |
| QBOOT_FACTORY_FLASH_LEN | FACTORY 自定义 flash 长度 |
| QBOOT_FLASH_ERASE_ALIGN | CUSTOM 后端擦除对齐大小 |
| QBOOT_USING_PRODUCT_CODE | 使用产品码验证 |
| QBOOT_PRODUCT_CODE | 产品码 |
| QBOOT_USING_AES | 使用 AES 解密 |
| QBOOT_AES_IV | AES 的 16 字节 IV |
| QBOOT_AES_KEY | AES 的 32 字节密钥 |
| QBOOT_USING_GZIP | 使用 gzip 解压 |
| QBOOT_USING_QUICKLZ | 使用 quicklz 解压 |
| QBOOT_USING_FASTLZ | 使用 fastlz 解压 |
| QBOOT_USING_HPATCHLITE | 使用 hpatchlite 差分升级 |
| QBOOT_HPATCH_PATCH_CACHE_SIZE | HPatchLite patch 缓存大小 |
| QBOOT_HPATCH_DECOMPRESS_CACHE_SIZE | HPatchLite 解压缓存大小 |
| QBOOT_HPATCH_USE_STORAGE_SWAP | HPatchLite 使用 flash swap 缓冲 |
| QBOOT_HPATCH_USE_RAM_BUFFER | HPatchLite 使用 RAM 缓冲 |
| QBOOT_HPATCH_SWAP_STORE_FAL | swap 使用 FAL |
| QBOOT_HPATCH_SWAP_STORE_CUSTOM | swap 使用自定义 flash |
| QBOOT_HPATCH_SWAP_STORE_FS | swap 使用文件系统 |
| QBOOT_HPATCH_SWAP_PART_NAME | swap FAL 分区名 |
| QBOOT_HPATCH_SWAP_FLASH_ADDR | swap 自定义 flash 地址 |
| QBOOT_HPATCH_SWAP_FLASH_LEN | swap 自定义 flash 长度 |
| QBOOT_HPATCH_SWAP_FILE_PATH | swap 文件路径 |
| QBOOT_HPATCH_SWAP_FILE_SIZE | swap 文件大小 |
| QBOOT_HPATCH_SWAP_OFFSET | swap 偏移 |
| QBOOT_HPATCH_COPY_BUFFER_SIZE | flash-to-flash 拷贝缓冲 |
| QBOOT_HPATCH_RAM_BUFFER_SIZE | RAM 缓冲大小 |
| QBOOT_USING_SHELL | 使用命令行功能 |
| QBOOT_SHELL_KEY_CHK_TMO | 进入 shell 的按键超时 |
| QBOOT_USING_SYSWATCH | 使用系统看守 |
| QBOOT_USING_OTA_DOWNLOADER | 使用 OTA downloader |
| QBOOT_USING_UPDATE_MGR | 启用 update 管理器 |
| QBOOT_POLL_DELAY_MS | update 轮询延时（ms） |
| QBT_UPDATE_MGR_PROGRESS_ENABLE | 下载进度输出开关 |
| QBOOT_UPDATE_MGR_USE_DOWNLOAD_HELPER | download helper（sign/擦除/写入） |
| QBOOT_USING_PRODUCT_INFO | 使用启动时产品信息输出 |
| QBOOT_PRODUCT_NAME | 产品名称 |
| QBOOT_PRODUCT_VER | 产品版本 |
| QBOOT_PRODUCT_MCU | 产品使用的 MCU |
| QBOOT_USING_STATUS_LED | 使用状态指示灯 |
| QBOOT_STATUS_LED_PIN | 指示灯引脚 |
| QBOOT_STATUS_LED_LEVEL | 指示灯点亮电平 |
| QBOOT_USING_FACTORY_KEY | 使用恢复出厂按键 |
| QBOOT_FACTORY_KEY_PIN | 按键引脚 |
| QBOOT_FACTORY_KEY_LEVEL | 按键按下电平 |
| QBOOT_FACTORY_KEY_CHK_TMO | 按键按下超时 |
| QBOOT_APP_RUN_IN_QSPI_FLASH | APP 运行在外部 QSPI |
| QBOOT_THREAD_STACK_SIZE | 线程栈大小 |
| QBOOT_THREAD_PRIO | 线程优先级 |
待实现：⬜ QBOOT_ALGO_CRYPT_XOR（加密算法）

### 2.4 弱函数（weak）需自行实现的场景

Qboot 将平台差异与产品策略放在 weak 钩子中，需要时在用户工程里提供同名实现覆盖。

- `qbt_jump_to_app()`：芯片/启动方式不同、需要关中断/外设/缓存或向量重映射时实现（platform/* 提供弱实现）。
- `qbt_fw_check()`：包头格式、签名、版本或安全校验策略不同，需要额外合法性校验时实现。
- `qbt_dest_part_verify()`：目标分区校验方式不同（CRC/签名/白名单）时实现。
- `qbt_release_sign_check()`：release 标记的存储位置/规则不同，或需要替换为签名标记时实现。
- `qboot_src_read_pos()`：固件头位置/长度非默认 `sizeof(fw_info_t)` 或放尾部时实现。
- `qbt_wdt_feed()`：系统无 syswatch 或需接入硬件看门狗时实现。
- `qbt_ops_custom_init()`：需要初始化自定义外设、注册 update_ops、定制后端时实现。
- `qboot_notify_update_result()`：需要持久化更新结果、上报日志时实现。

### 2.5 custom_ops（自定义存储后端）

`qboot_custom_ops.c` 提供 CUSTOM 后端，用于不依赖 FAL/FS 的裸 Flash/外设存储。

- 适用场景：无 FAL/文件系统，或需要直接操作片上/外置 Flash；APP/DOWNLOAD/FACTORY 映射到固定地址区间。
- 启用方式：使能 `QBOOT_PKG_SOURCE_CUSTOM`，并选择 `QBOOT_*_STORE_CUSTOM`。
- 必要实现：覆盖 `qbt_custom_flash_read/write/erase` 弱函数，否则 CUSTOM 后端会返回错误。
- 必要配置：`QBOOT_*_FLASH_ADDR/LEN` 与 `QBOOT_FLASH_ERASE_ALIGN`。

### 2.6 update 管理器

update 管理器负责等待窗口、下载超时、原因持久化与下载统计。

- 使能 `QBOOT_USING_UPDATE_MGR`，并根据需要打开 `QBOOT_UPDATE_MGR_USE_DOWNLOAD_HELPER` 与 `QBT_UPDATE_MGR_PROGRESS_ENABLE`。
- 在 `qbt_ops_custom_init()` 中注册 `qbt_update_mgr_register()`，提供 `qbt_update_ops_t` 回调。
- 传输协议在下载开始/数据/结束时调用 `qbt_update_mgr_download_begin/write/on_finish`（开启 helper）或 `qbt_update_mgr_on_start/on_data/on_finish`（未开启 helper）。
- 示例：通过协议触发下载并注册 update_ops，详见 `doc/qboot_update.md`。

### 2.7 各功能模块资源使用情况，详见 ：[qboot各项配置资源占用情况说明](https://gitee.com/qiyongzhong0/rt-thread-qboot/blob/master/doc/QBoot%E5%90%84%E9%A1%B9%E9%85%8D%E7%BD%AE%E8%B5%84%E6%BA%90%E5%8D%A0%E7%94%A8%E6%83%85%E5%86%B5%E8%AF%B4%E6%98%8E.md)

### 2.8 如何使用QBoot组件快速制作bootloader，详见：[QBoot使用指导](https://gitee.com/qiyongzhong0/rt-thread-qboot/blob/master/doc/QBoot%E4%BD%BF%E7%94%A8%E6%8C%87%E5%AF%BC.md)

### 2.9 差分升级使用说明, 详见：[QBootHpatchLite使用说明](https://gitee.com/qiyongzhong0/rt-thread-qboot/blob/master/doc/QBootHpatchLite%E4%BD%BF%E7%94%A8%E8%AF%B4%E6%98%8E.md)

## 3. 联系方式

* 维护：qiyongzhong
* 主页：https://github.com/qiyongzhong0/rt-thread-qboot
* 主页：https://gitee.com/qiyongzhong0/rt-thread-qboot
* 邮箱：917768104@qq.com

