# Quick bootloader v1.0.0 各功能模块资源使用情况列表
---
### 注：
### - 表中flash和ram数值的单位为字节。
### - 其中`(+4096)`表示单独打开此项时会增加4096字节ram使用，如果多个带有`(+4096)`的项被同时打开时，总共只增加4096字节，`(+1024)`同理。
### - 表中所列ram占用不包括线程栈的使用。
### - 最小功能时flash使用约4.8k，ram使用约4.1k；全功能时flash使用约39.4k，ram使用约17.8k。
### - 列表所示是使用RT-Thread Studio编译器，在优化选项-Os下，编译所得测试结果。
---

|功能模块|选项宏|flash|ram|说明|使用推荐|
|----|----|----|----|----|---|
|Qboot|PKG_USING_QBOOT|4952|4196|组件核心功能模块|必备|
|Syswatch|QBOOT_USING_SYSWATCH|2636|96|使用系统看守组件，以保障系统运行安全|安全第一，推荐使用|
|Factory key|QBOOT_USING_FACTORY_KEY|76|0|使用恢复出厂按键功能|有按键时，推荐使用|
|Status led|QBOOT_USING_STATUS_LED|604+376|20|使用运行状态指示功能|方便识别状态，推荐使用|
|verify|QBOOT_USING_VERIFY|11560|296|使用代码校验功能，校验升级代码正确性|资源占用略大，视需求使用|
|AES|QBOOT_USING_AES|11560|296 (+4096)|使用AES解密功能|资源占用略大，视需求使用|
|gzip|QBOOT_USING_GZIP|9856|8256 (+4096)|使用gzip解压缩功能|压缩率较好，资源占用大，产品资源丰富且固件尺寸较大时，推荐使用|
|Quicklz|QBOOT_USING_QUICKLZ|768|4 (+1024) (+4096)|使用Quicklz解压缩功能|压缩率中，推荐使用|
|Fastlz|QBOOT_USING_FASTLZ|768|0 (+1024) (+4096)|使用Fastlz解压缩功能|压缩率略差，不推荐|
|OTA_download|QBOOT_USING_OTA_DOWNLOAD|2612|16|使用固件包下载功能，支持通过串口使用ymodem协议下载固件到flash，此功能依赖shell|资源足够时，推荐使用|
|shell|QBOOT_USING_SHELL|5860|284|使用shell命令行功能, 支持固件检测、克隆、恢复、校验等功能，此功能依赖shell|资源足够时，推荐使用|
|Product info|QBOOT_USING_PRODUCT_INFO|140|0|使用启动时输出产品信息，flash占用与用户显示信息多少有关|视需求使用|