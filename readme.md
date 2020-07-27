# Quick bootloader

## 1.简介

**Quick bootloader** 是一个用于快速制作bootloader的组件。

### 1.1目录结构

`Quick bootloader` 软件包目录结构如下所示：

``` 
qboot
├───doc                                // 说明文档目录
│   │   qboot各项配置资源占用情况说明.md  // 资源占用说明
│   │   qboot工作流程说明.md            // 工作流程说明
│   └───qboot状态指示灯说明.md          // 状态指示灯说明
├───inc                               // 头文件目录
│   │   qboot.h                       // 主模块头文件
│   │   qboot_aes.h                   // aes解密模块头文件
│   │   qboot_fastlz.h                // fastlz解压模块头文件
│   │   qboot_gzip.h                  // gzip解压模块头文件
│   └───qboot_quicklz.h     	      // quicklz解压模块头文件
├───src                               // 源码目录
│   │   qboot.c                       // 主模块
│   │   qboot_aes.c                   // aes解密模块
│   │   qboot_fastlz.c                // fastlz解压模块
│   │   qboot_gzip.c                  // gzip解压模块
│   └───qboot_quicklz                 // quicklz解压模块
│   license                           // 软件包许可证
│   readme.md                         // 软件包使用说明
└───SConscript                        // RT-Thread 默认的构建脚本
```

### 1.2许可证

Quick bootloader 遵循 LGPLv2.1 许可，详见 `LICENSE` 文件。

### 1.3依赖

- RT_Thread 4.0
- fal
- crclib

## 2.使用

### 2.1获取组件

- **方式1：**
通过 *Env配置工具* 或 *RT-Thread studio* 开启软件包，根据需要配置各项参数；配置路径为 *RT-Thread online packages -> system -> qboot* 

### 2.2功能选项宏定义说明

| 选项宏 | 说明 |
| ---- | ---- |
| QBOOT_APP_PART_NAME 	    | 应用代码使用的fal分区名称
| QBOOT_DOWNLOAD_PART_NAME 	| 下载固件使用的fal分区名称
| QBOOT_FACTORY_PART_NAME 	| 出厂固件使用的fal分区名称
| QBOOT_USING_PRODUCT_CODE 	| 使用产品码验证，防止非法升级
| QBOOT_PRODUCT_CODE 	    | 定义产品码
| QBOOT_USING_AES 		    | 使用AES解密功能
| QBOOT_AES_IV 		    	| AES的16字节初始向量
| QBOOT_AES_KEY 		    | AES的32字节密钥
| QBOOT_USING_GZIP 			| 使用gzip解压缩功能
| QBOOT_USING_QUICKLZ 		| 使用quicklz解压缩功能
| QBOOT_USING_FASTLZ 		| 使用fastlz解压缩功能
| QBOOT_USING_SHELL 		| 使用命令行功能
| QBOOT_SHELL_KEY_CHK_TMO 	| 等待用户按键进入shell的超时时间
| QBOOT_USING_SYSWATCH 		| 使用系统看守组件
| QBOOT_USING_OTA_DOWNLOAD 	| 使用OTA downloader组件
| QBOOT_USING_PRODUCT_INFO 	| 使用启动时产品信息输出
| QBOOT_PRODUCT_NAME 	    | 产品名称
| QBOOT_PRODUCT_VER 	    | 产品版本
| QBOOT_PRODUCT_MCU 	    | 产品使用的MCU
| QBOOT_USING_STATUS_LED 	| 使用状态指示灯
| QBOOT_STATUS_LED_PIN 	    | 指示灯使用的引脚
| QBOOT_STATUS_LED_LEVEL 	| 指示灯的点亮电平
| QBOOT_USING_FACTORY_KEY 	| 使用恢复出厂按键
| QBOOT_FACTORY_KEY_PIN 	| 按键使用的引脚
| QBOOT_FACTORY_KEY_LEVEL 	| 按键按下后的引脚电平
| QBOOT_FACTORY_KEY_CHK_TMO | 检测按键持续按下的超时时间

### 2.3各功能模块资源使用情况

|功能模块|选项宏|flash|ram|说明|使用推荐|
|----|----|----|----|----|---|
|Qboot|PKG_USING_QBOOT|5392|4192|组件核心功能模块|必备|
|Syswatch|QBOOT_USING_SYSWATCH|2812|100|使用系统看守组件，以保障系统运行安全|安全第一，推荐使用|
|Factory key|QBOOT_USING_FACTORY_KEY|80|0|使用恢复出厂按键功能|有按键时，推荐使用|
|Status led|QBOOT_USING_STATUS_LED|980|20|使用运行状态指示功能|方便识别状态，推荐使用|
|Product code|QBOOT_USING_PRODUCT_CODE|120|0|使用产品码鉴别功能|防止非法固件升级，推荐使用|
|AES|QBOOT_USING_AES|11568|296 (+4096)|使用AES解密功能|资源占用略大，视需求使用|
|gzip(laster)|QBOOT_USING_GZIP|9972|8268 (+4096)|使用gzip解压缩功能|压缩率较好，资源占用大，产品资源丰富且固件尺寸较大时，推荐使用|
|Quicklz|QBOOT_USING_QUICKLZ|768|4 (+1024) (+4096)|使用Quicklz解压缩功能|压缩率中，推荐使用|
|Fastlz|QBOOT_USING_FASTLZ|704|0 (+1024) (+4096)|使用Fastlz解压缩功能|压缩率略差，不推荐|
|OTA_download|QBOOT_USING_OTA_DOWNLOAD|2456|24|使用固件包下载功能，支持通过串口使用ymodem协议下载固件到flash，此功能依赖shell|资源足够时，推荐使用|
|shell|QBOOT_USING_SHELL|3268|8|使用shell命令行功能, 支持固件检测、克隆、恢复、校验等功能，此功能依赖shell|资源足够时，推荐使用|
|Product info|QBOOT_USING_PRODUCT_INFO|164|0|使用启动时输出产品信息，flash占用与用户显示信息多少有关|视需求使用|

#### 注：
#### - 表中flash和ram数值的单位为字节。
#### - 其中`(+4096)`表示单独打开此项时会增加4096字节ram使用，如果多个带有`(+4096)`的项被同时打开时，总共只增加4096字节，`(+1024)`同理。
#### - 表中所列ram占用不包括线程栈的使用。
#### - 最小功能时flash使用约5.3k，ram使用约4.1k；全功能时flash使用约37.4k，ram使用约17.7k。
#### - 列表所示是使用RT-Thread Studio编译器，在优化选项-Os下，编译所得测试结果。

## 3. 联系方式

* 维护：qiyongzhong
* 主页：https://gitee.com/qiyongzhong0/rt-thread-qboot

