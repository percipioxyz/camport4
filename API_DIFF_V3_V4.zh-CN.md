# Camport V3/V4 API 差异说明（中文用户版）

## 文档目的

本说明面向最终用户，回答三个问题：

1. V4 是否完整包含 V3 API。
2. V3 独有 API 分别做什么。
3. V4 是否有可替代的实现路径。

## 一页结论

- V4 **几乎完全包含** V3 API。
- 导出 API 数量：V3=`88`，V4=`123`。
- V3 独有 API：`16` 个。
- 其中：
  - 日志类 `4` 个：V4 有近似替代（模型变化）。
  - 深度空洞填补 `1` 个：V4 有直接替代（且参数更灵活）。
  - TYISP 类 `11` 个：V3仅适用于没有硬件ISP的一个相机，V4 改为“解码接口 + GenICam 参数接口”。

## V3 独有 16 个 API（功能与 V4 对应关系）

说明：

- `可替代`: V4 有同类能力。
- `部分替代`: 能实现近似目标，但行为/模型不完全一致。
- `无直接替代`: 头文件中无对应 API。

| API | V3 功能 | V4 对应 |
|---|---|---|
| `TYAppendLogToFile` | 添加文件日志并设级别（单接口） | 可替代：`TYCfgLogFile` + `TYSetLogLevel` + `TYEnableLog` |
| `TYRemoveLogFile` | 按文件路径移除日志目标 | 部分替代：`TYDisableLog(TY_LOG_TYPE_FILE)`（按类型停用） |
| `TYAppendLogToServer` | 添加 TCP/UDP 服务器日志并设级别 | 可替代：`TYCfgLogServer` + `TYSetLogLevel` + `TYEnableLog` |
| `TYRemoveLogServer` | 按协议/IP/端口移除服务器日志目标 | 部分替代：`TYDisableLog(TY_LOG_TYPE_SERVER)` |
| `TYDepthImageFillEmptyRegion` | 深度图空洞填补 | 可替代：`TYDepthImageInpainter` |
| `TYISPCreate` | 创建软件 ISP 句柄 | 无直接替代 |
| `TYISPRelease` | 释放软件 ISP 句柄 | 无直接替代 |
| `TYISPLoadConfig` | 加载 ISP 配置块 | 部分替代：通过 `TYParameter.h` 写设备特征（非同一模型） |
| `TYISPUpdateDevice` | 同步 ISP 与设备状态 | 无直接替代 |
| `TYISPSetFeature` | 通过 `TY_ISP_FEATURE_ID` 设置 ISP 特征 | 部分替代：`TYIntegerSetValue`/`TYFloatSetValue`/`TYEnumSetString` 等 |
| `TYISPGetFeature` | 通过 `TY_ISP_FEATURE_ID` 获取 ISP 特征 | 部分替代：`TYIntegerGetValue`/`TYFloatGetValue`/`TYEnumGetString` 等 |
| `TYISPGetFeatureSize` | 获取 ISP 特征数据长度 | 部分替代：`TYByteArrayGetSize`/`TYStringGetLength` |
| `TYISPHasFeature` | 检查 ISP 特征是否存在 | 可替代：`TYParamExist` |
| `TYISPGetFeatureInfoList` | 获取 ISP 特征信息列表 | 部分替代：逐项查询 `TYParamGetType/Access/...` |
| `TYISPGetFeatureInfoListSize` | 获取 ISP 特征列表缓冲区大小 | 无直接替代 |
| `TYISPProcessImage` | 以 ISP 管线处理 Bayer/raw 图像输出 | 部分替代：`TYGetDecodeTargetPixFmt` + `TYGetDecodeBufferSize` + `TYDecodeImage` |

## 需要用户重点关注的变化

### 1) 日志模型变化

- V3 偏向“追加/移除具体目标”。
- V4 偏向“先配置，再按类型启停”。

### 2) ISP 模型变化

- V3 提供完整 TYISP 生命周期与特征 ID 控制。
- V4 取消 TYISP 导出接口(V3 TYISP仅适用于没有硬件ISP的一个相机)，建议使用：
  - 图像解码转换：`TYDecodeImage` 路径。
  - 设备特征调参：`TYParameter.h` 的 GenICam 风格接口。

### 3) 参数接口升级

- V4 新增 `TYParameter.h`，支持按类型读写和特征元信息查询。
- 对新设备（GenICam/SFNC）更友好。

## V4 新增能力概览（用户视角）

- 日志：支持按类型启停，文件日志支持轮转配置。
- 参数：支持类型安全访问（整型/浮点/枚举/字符串/字节数组/命令）。
- 解码：统一输出格式选择与输出缓冲区预估，便于跨格式处理。
- 坐标映射：多项映射函数从 V3 inline 工具升级为导出 C API。

## 参考文档

- 主差异文档（英文详版）：`API_DIFF_V3_V4.md`
- 迁移代码示例：`API_MIGRATION_V3_TO_V4.md`
