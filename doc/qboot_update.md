# qboot_update.c 说明

## 1. 模块职责

- 管理升级原因与运行状态（WAIT/RECV/READY）。
- 处理等待窗口与下载空闲超时，决定跳转 APP 或继续等待。
- 提供 download helper（`QBOOT_UPDATE_MGR_USE_DOWNLOAD_HELPER`），统一处理 download 分区的打开/擦除/写入。
- 当 APP 无效且超时，调用 `try_recover` 尝试从 DOWNLOAD/FACTORY 恢复。
- 维护下载进度统计与日志输出（可选）。

## 2. 使用条件

- 使能 `QBOOT_USING_UPDATE_MGR`。
- 提供 `qbt_update_ops_t` 回调：`is_app_valid/get_reason/set_reason/enter_download/leave_download/on_error/on_ready_to_app/try_recover`。
  - `try_recover` 必须实现（可以返回 `RT_FALSE` 作为占位）。
- 需要持久化更新原因（EEPROM/Flash 等），应用侧可写入 `QBT_UPD_REASON_REQ` 后重启触发升级。
- `qbt_update_mgr_download_begin/write` 仅在 `QBOOT_UPDATE_MGR_USE_DOWNLOAD_HELPER` 使能时可用。

## 3. 接入流程

1. 实现 update_ops 回调并注册：`qbt_update_mgr_register(&ops, wait_ms, idle_ms)`。
2. 主流程调用 `qbt_update_mgr_poll(QBOOT_POLL_DELAY_MS)`。
3. 传输协议触发下载：
   - helper 模式：`qbt_update_mgr_download_begin()` -> 循环 `qbt_update_mgr_download_write()` -> `qbt_update_mgr_on_finish(ok)`。
   - 非 helper：自行打开/擦除/写入目标分区，同时调用 `qbt_update_mgr_on_start/on_data/on_finish` 更新状态。
4. `on_finish(ok)` 为真时进入 READY，qboot 后续执行 release/apply 与跳转。
5. `qboot_notify_update_result()` 用于持久化更新结果（成功写 DONE，失败写 IN_PROGRESS）。

## 4. 进入条件与状态机

- 启动时根据 `get_reason()` 决定：
  - `QBT_UPD_REASON_NONE/DONE`：若 APP 有效 -> READY，否则 WAIT。
  - `QBT_UPD_REASON_REQ/IN_PROGRESS`：进入 WAIT。
- WAIT 超时：APP 有效则 READY，否则尝试 `try_recover()`（每个下载会话只探测一次），成功则 READY，失败继续等待。
- RECV 空闲超时：APP 有效则 READY，否则尝试 `try_recover()`（每个下载会话只探测一次），成功则 READY，失败回 WAIT。

## 5. download helper 行为（QBOOT_UPDATE_MGR_USE_DOWNLOAD_HELPER）

- `download_begin`：打开 DOWNLOAD、清除 release sign、擦除分区，进入 RECV。
- `download_write`：写入数据块，刷新进度/时间戳。
- `on_finish(ok)`：内部设置 download_ok=ok；ok 为真则进入 READY。
- helper 模式下，`qboot_src_read_pos()` 返回 0，按“裸固件”流处理。
- `qbt_fw_check()` 仅依赖 download_ok；默认使用分区长度作为 raw/pkg size。
  - 若需要精确长度或 CRC 校验，应在协议层校验后决定 `on_finish(ok)`，或自行修改 `qbt_fw_check()` 逻辑。

## 6. try_recover 回调

- 当 APP 无效且超时，update_mgr 调用 `try_recover()`。
- 回调返回 `RT_TRUE` 表示 DOWNLOAD/FACTORY 可用，允许进入 READY；返回 `RT_FALSE` 则继续等待。
- 一次性探测按“下载会话”生效：同一会话内只调用一次，避免反复探测刷屏。
- 进入新的下载会话（`on_start` 进入 RECV）后，会重置探测机会。
- 可直接使用 `qbt_update_mgr_try_recover()`（helper 内置），或在 bootloader 中自定义检查策略。

## 7. 示例（f407_boot/UserSrc/bootloader.c）

```c
static rt_bool_t bootloader_try_recover(void)
{
    return qbt_update_mgr_try_recover();
}

static const qbt_update_ops_t s_update_ops = {
    .is_app_valid    = bootloader_is_app_valid,
    .get_reason      = bootloader_get_reason,
    .set_reason      = bootloader_set_reason,
    .enter_download  = bootloader_enter_download,
    .leave_download  = bootloader_leave_download,
    .on_error        = bootloader_on_error,
    .on_ready_to_app = bootloader_on_ready_to_app,
    .try_recover     = bootloader_try_recover,
};

rt_bool_t qbt_ops_custom_init(void)
{
    qbt_update_mgr_register(&s_update_ops, BL_WAIT_DOWNLOAD_MS, BL_DOWNLOAD_IDLE_MS);
    return RT_TRUE;
}

enum kwp_response_code kwp_download_request(struct kwp_msg *msg)
{
    RT_UNUSED(msg);
    return qbt_update_mgr_download_begin() ? KWP_RESPONSE_CODE_SUCCESS : KWP_RESPONSE_CODE_ACCESSDENIED;
}

enum kwp_response_code kwp_download(struct kwp_msg *msg)
{
    if (!qbt_update_mgr_download_write(PageIndex * BL_DOWNLOAD_SIZE, (rt_uint8_t *)&msg->data[1], BL_DOWNLOAD_SIZE))
    {
        return KWP_RESPONSE_CODE_DOWNLOADNOTACCEPTED;
    }
    PageIndex++;
    return KWP_RESPONSE_CODE_SUCCESS;
}

enum kwp_response_code kwp_download_finish(struct kwp_msg *msg)
{
    RT_UNUSED(msg);
    qbt_update_mgr_on_finish(RT_TRUE);
    return KWP_RESPONSE_CODE_SUCCESS;
}
```

## 8. 进度输出

- `QBT_UPDATE_MGR_PROGRESS_ENABLE` 使能后，默认每 500ms 输出一次下载进度。
- 若希望输出真实包大小，可在收到包头后调用 `qbt_update_mgr_set_total(actual_size)`。
- `wait_ms/idle_ms` 传 0 表示不启用超时。
