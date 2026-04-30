# QBoot使用指导 —— 集成HPatchLite实现差分升级（原地更新 In-place OTA）

## 1. 功能简介

本文档在《QBoot使用指导》的基础上，说明如何集成与使用 **HPatchLite** 实现**差分升级（FOTA）**。

本实现为**原地更新（in-place）**：旧固件所在分区（old/app）会被直接覆盖为新固件。为解决“同一分区读旧数据 + 写新数据”的冲突，提供两种缓冲策略，并通过 Kconfig 二选一配置：

* **FLASH SWAP**：使用独立存储区域（分区/文件/自定义区域）作为周转缓冲，RAM 占用小但速度较慢。
* **RAM BUFFER**：使用 RAM 作为周转缓冲，速度快但 RAM 占用较大。

实现特点：

* 统一的 **swap 缓冲对象 `qbt_swap_t`**：进入 patch 流程前统一分配 `ram_buf`

  * FLASH SWAP：`ram_buf` 用作 flash-to-flash 拷贝的临时缓存
  * RAM BUFFER：`ram_buf` 用作主缓冲区
* 统一的“**写入 swap -> swap 满则 commit**”流程：`_do_write_new()` 内部按容量分块写入，满后 `qbt_swap_commit()` 提交到 old 分区。
* 写入前按擦除对齐执行 **增量擦除**：`qbt_erase_aligned_range()` 维护 `old_part_erased_end`，避免重复擦除与越界擦除。
* 更新完成后执行**尾部对齐擦除**（新固件比分区小）：避免残留旧数据影响校验/运行。
* 最终以 `committed_len == newer_file_len` 做长度一致性检查。

---

## 2. 差分升级包制作流程

差分升级的核心在于制作一个包含固件差异信息的“补丁包”。整个流程分为两步：

### 2.1 生成原始Patch文件

这一步需要在自行下载工具完成，用于对比新旧固件，并生成最原始的差分数据。

1.  **下载工具**:
    从HPatchLite的官方GitHub仓库下载其预编译的二进制工具包：
    [https://github.com/sisong/HPatchLite/releases](https://github.com/sisong/HPatchLite/releases)
    请下载最新版本的 `HPatchLite_vX.X.X_bin_xxxx.zip` 并解压。

2.  **准备文件**:
    *   `old.bin`: 您设备上当前正在运行的固件版本。
    *   `new.bin`: 您希望升级到的新版本固件。

3.  **执行打包命令**:
    打开命令行工具（如CMD或PowerShell），进入 `hdiffi.exe` 所在的目录，执行以下命令：

    ```bash
    .\hdiffi.exe old.bin new.bin patch.bin
    ```    *   `old.bin`: 旧固件文件路径。
    *   `new.bin`: 新固件文件路径。
    *   `patch.bin`: 命令成功后生成的原始差分补丁文件。

4.  **（可选）验证差分包**:
    您可以使用 `hpatchi.exe` 工具在PC上模拟差分合并过程，以验证 `patch.bin` 的正确性。

    ```bash
    .\hpatchi.exe old.bin patch.bin new_flash.bin
    ```
    执行后，使用文件比对工具确认生成的 `new_flash.bin` 与 `new.bin` 完全一致。

### 2.2 将Patch文件打包为QBoot可识别的RBL格式

QBoot需要一个带有标准头部信息（`.rbl`格式）的升级包。我们需要使用配套的Python脚本将上一步生成的 `patch.bin` 进行封装。

1.  **获取脚本**:
    该脚本通常位于 `qboot/tools/package_tool.py`。
    脚本参考了 [RT_ota_packager_python](https://github.com/sunkr1995/RT_ota_packager_python) 的设计。

2.  **执行打包命令**:
    在PC上执行以下Python脚本命令：
    ```bash
    python .\package_tool.py patch.bin new.bin
    ```
    *   **第一个参数 (`patch.bin`)**: 上一步生成的原始差分补丁文件。它将作为RBL包的数据主体。
    *   **第二个参数 (`new.bin`)**: 新版本固件文件。**注意**：这个文件**仅用于**计算RBL头部所需的元数据（如新固件的总大小`raw_size`和CRC校验`raw_crc`），它**不会**被包含在最终的升级包里。

    命令成功后，会在同目录下生成一个名为 **`patch.rbl`** 的文件。这个文件就是最终可以被QBoot下载并用于差分升级的固件包。

---

## 3. QBoot 组件配置（Kconfig）

### 3.1 启用HPatchLite
首先，在QBoot的配置菜单中，必须勾选 `using hpatch-lite decompress` 选项。
```Kconfig
config QBOOT_USING_HPATCHLITE
    bool "using hpatch-lite decompress"
    select PKG_USING_HPATCHLITE
    default n
```


### 3.2 选择原地更新策略（二选一）

```Kconfig
choice
    prompt "HPatchLite In-Place Update Strategy"
    default QBOOT_HPATCH_USE_STORAGE_SWAP

    config QBOOT_HPATCH_USE_STORAGE_SWAP
        bool "Use a storage medium as swap buffer"

    config QBOOT_HPATCH_USE_RAM_BUFFER
        bool "Use a RAM buffer (Faster, consumes RAM)"
endchoice
```

两种策略的共同点：

* 统一使用 `qbt_swap_t` 管理 swap 行为：`init/deinit/append/copy_to_old/reset`
* `ram_buf` 在进入 patch 流程前统一 `rt_malloc()` 分配（由 `qbt_swap_alloc_buf()` 完成）
* `_do_write_new()` 只负责“写入 swap”，swap 满则触发 `qbt_swap_commit()`

---

## 4. 策略一：FLASH SWAP（使用存储介质做 swap）

### 4.1 适用场景

* RAM 资源紧张
* 允许牺牲一定速度换取更低 RAM 占用

### 4.2 配置项

#### 4.2.1 选择 swap 后端（FAL / 自定义 / 文件系统）

```Kconfig
if QBOOT_HPATCH_USE_STORAGE_SWAP
    choice
        prompt "HPatchLite swap backend"
        default QBOOT_HPATCH_SWAP_STORE_FAL

        config QBOOT_HPATCH_SWAP_STORE_FAL
            bool "FAL partition"

        config QBOOT_HPATCH_SWAP_STORE_CUSTOM
            bool "Custom flash region"

        config QBOOT_HPATCH_SWAP_STORE_FS
            bool "Filesystem file"
    endchoice
endif
```

#### 4.2.2 swap 区域与偏移

* FAL 分区后端：

```Kconfig
config QBOOT_HPATCH_SWAP_PART_NAME
    string "Swap partition name for HPatchLite"
    default "swap"
```

* 文件系统后端：

```Kconfig
config QBOOT_HPATCH_SWAP_FILE_PATH
    string "Swap file path for HPatchLite"
    default "/hpatch.swap"

config QBOOT_HPATCH_SWAP_FILE_SIZE
    int "Swap file size for HPatchLite"
    default 524288
```

并且实现中会检查：

* `QBOOT_HPATCH_SWAP_FILE_SIZE` 必须 **>= 目标分区擦除对齐大小**（即 `old_part_sector_size`）

* swap 起始偏移：

```Kconfig
config QBOOT_HPATCH_SWAP_OFFSET
    int "Offset within the swap region to use as a buffer"
    default 0
```

#### 4.2.3 flash-to-flash 拷贝临时 RAM 缓冲

```Kconfig
config QBOOT_HPATCH_COPY_BUFFER_SIZE
    int "RAM buffer size for flash-to-flash copy operations"
    default 4096
```

实现含义：

* FLASH SWAP 模式下，patch 产生的新数据先写入 swap（分区/文件）
* 提交（commit）时，从 swap 读出，再写入 old 分区
* 读写搬运使用 `ram_buf` 作为临时拷贝缓存（大小由 `QBOOT_HPATCH_COPY_BUFFER_SIZE` 决定）

### 4.3 关键实现点

* `qbt_flash_swap_init()`：

  * 打开 swap 目标（分区/文件/自定义区域）
  * 按 `QBOOT_HPATCH_SWAP_OFFSET` 与可用容量计算 `capacity`
  * 先擦除 swap 区域，`write_pos=0`
* `qbt_flash_swap_append()`：把新数据写入 swap 存储介质
* `qbt_flash_copy_to_old()`：按 `ram_buf_size` 分块从 swap 读出并写入 old 分区
* `qbt_flash_swap_reset()`：每次 commit 后擦除 swap 区域，为下一轮写入做准备

---

## 5. 策略二：RAM BUFFER（使用 RAM 做 swap）

### 5.1 适用场景

* RAM 资源充足
* 希望更快速度、更少 flash 擦写次数

### 5.2 配置项

```Kconfig
config QBOOT_HPATCH_RAM_BUFFER_SIZE
    int "RAM buffer size for HPatchLite (must be >= sector size)"
    default 4096
```

建议：

* `QBOOT_HPATCH_RAM_BUFFER_SIZE` **>= app/old 分区的最小擦除单元**（sector size / erase align）
* 缓冲越大，commit 次数越少，整体速度可能更好，但 RAM 占用更高

### 5.3 关键实现点

* `qbt_ram_swap_append()`：把新数据 `memcpy` 到 `ram_buf`
* `qbt_ram_copy_to_old()`：commit 时直接把 `ram_buf` 写入 old 分区
* `qbt_ram_swap_reset()`：commit 后仅 `write_pos=0`，无需擦除 swap 区域

---

## 6. 存储后端与擦除对齐

HPatchLite 差分流程通过 `qboot_io_ops_t` 操作存储后端，后端需要实现：

* `open/close/read/erase/write/size`
* 推荐实现 `ioctl` 扩展点：

### 6.1 擦除对齐查询：`QBOOT_IO_CMD_GET_ERASE_ALIGN`

差分原地更新中必须考虑擦除对齐：

* commit 写入 old 分区前，使用 `qbt_erase_aligned_range()` 按对齐增量擦除
* 更新完成后，对新固件尾部执行对齐擦除

因此后端建议实现：

* `ioctl(old_part, QBOOT_IO_CMD_GET_ERASE_ALIGN, &erase_align)`

当前实现中如果获取失败会直接报错退出（建议在后端补齐该 ioctl 以保证兼容性）。

---

## 7. FAL 分区与配置示例（以 FLASH SWAP 为例）

### 7.1 fal_cfg.h 分区规划示例

示例（假设擦除粒度 128KB，分区规划 app/factory/swap/download）：

```c
#define FAL_PART_TABLE                                                                                  \
{                                                                                                       \
    {FAL_PART_MAGIC_WORD,   "app",          "onchip_flash_128k",                 0, 3 * 128 * 1024, 0}, \
    {FAL_PART_MAGIC_WORD,   "factory",      "onchip_flash_128k",    3 * 128 * 1024, 2 * 128 * 1024, 0}, \
    {FAL_PART_MAGIC_WORD,   "swap",         "onchip_flash_128k",    5 * 128 * 1024, 1 * 128 * 1024, 0}, \
    {FAL_PART_MAGIC_WORD,   "download",     "onchip_flash_128k",    6 * 128 * 1024, 1 * 128 * 1024, 0}, \
}
```

要点：

* `app`：差分升级目标分区（old/new 都在这里）
* `swap`：FLASH SWAP 策略必须存在，且建议 **>= app 的擦除粒度**
* `download`：存放 `patch.rbl`

### 7.2 rtconfig.h示例

```c
#define PKG_USING_QBOOT
#define QBOOT_USING_PRODUCT_CODE
#define QBOOT_PRODUCT_CODE "00010203040506070809"
#define QBOOT_APP_PART_NAME "app"
#define QBOOT_DOWNLOAD_PART_NAME "download"
#define QBOOT_FACTORY_PART_NAME "factory"
#define QBOOT_USING_QUICKLZ

/* --- HPatchLite 关键配置 --- */
#define QBOOT_USING_HPATCHLITE

/* --- HPatchLite 策略：FLASH SWAP --- */
#define QBOOT_HPATCH_USE_STORAGE_SWAP
#define QBOOT_HPATCH_SWAP_PART_NAME "swap"
#define QBOOT_HPATCH_SWAP_OFFSET 0
#define QBOOT_HPATCH_COPY_BUFFER_SIZE 4096

/* Patch/Decompress cache（与 Kconfig 一致） */
#define QBOOT_HPATCH_PATCH_CACHE_SIZE 4096
#define QBOOT_HPATCH_DECOMPRESS_CACHE_SIZE 4096
```

---

## 8. 差分升级执行流程（实现行为对照）

以下描述对应当前实现的关键函数与状态字段：

1. **初始化实例**

   * `qbt_hpatchlite_release_from_part()`
   * 填充 `hpatchi_instance_t`：`patch_part/old_part/newer_file_len/patch_file_len` 等
   * 通过 `ioctl(QBOOT_IO_CMD_GET_ERASE_ALIGN)` 获取 `old_part_sector_size`
   * 分配 `swap->ram_buf`：`qbt_swap_alloc_buf()`
   * 初始化 swap 后端：`swap->init()`

2. **进入 HPatchLite 合成**

   * 调用 `hpi_patch(&instance.parent, ..., _do_read_patch, _do_read_old, _do_write_new)`
   * `_do_read_patch()`：按流式 offset 读取 patch 数据
   * `_do_read_old()`：直接从 old 分区读取旧数据
   * `_do_write_new()`：把新数据写入 swap；当 `write_pos >= capacity` 时触发 `qbt_swap_commit()`

3. **提交逻辑（统一）**

   * `qbt_swap_commit()`

     * 计算本轮 `used = write_pos`
     * `qbt_erase_aligned_range()`：对 `[committed_len, committed_len+used)` 做对齐擦除（增量）
     * `swap->copy_to_old()`：把 swap 数据写入 old 分区
     * 更新 `committed_len += used`
     * `swap->reset()` 清理本轮状态

4. **结束收尾**

   * 若 patch 成功且 swap 仍有残留：再 commit 一次
   * 若新固件小于分区：对尾部 `[newer_file_len, part_len)` 做对齐擦除（`align_start_up = RT_TRUE`）
   * 最终检查：`committed_len == newer_file_len` 判定成功

---

## 9. 常见注意事项

* **擦除对齐必须正确**：后端建议实现 `QBOOT_IO_CMD_GET_ERASE_ALIGN`，否则会导致擦除范围不正确或流程直接失败。
* **RAM BUFFER 大小建议 >= 擦除单元**：避免跨擦除块写入带来的原地更新风险。
* **FLASH SWAP 容量规划**：swap 区域可用容量由 `swap_part_size - QBOOT_HPATCH_SWAP_OFFSET` 决定，需满足差分过程周转需求（至少覆盖算法需要的写缓冲提交粒度）。
* **文件系统 swap**：需要保证 `QBOOT_HPATCH_SWAP_FILE_SIZE >= erase_align`，否则会直接拒绝执行。

---
