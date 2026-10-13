# macOS 安装包体积精简

## 原因与处理

macOS 发布脚本分别构建 `arm64` 与 `x86_64`，但 Qt 6.11.1 的部署库和插件为双架构二进制。只设置 `CMAKE_OSX_ARCHITECTURES` 不会裁剪这些预编译依赖。

安装阶段现在会按照 `CMAKE_OSX_ARCHITECTURES` 裁剪安装副本中 Mach-O 文件的多余架构，然后重新执行 ad-hoc 签名。不会修改 Qt 安装目录、构建目录或用户数据。显式双架构构建保留两种架构；没有指定架构的构建不裁剪。缺少目标架构时直接终止打包，避免生成无法运行的安装包。

未移除 QML 风格、图像格式、TLS、SQLite、翻译或浏览器脚本编辑器资源。Windows 和 Linux 打包行为不变。

## 本机验证（2026-10-14）

环境：Apple Silicon，Qt 6.11.1，Release，`CMAKE_OSX_ARCHITECTURES=arm64`，`BUILD_TESTING=OFF`。先按修改前规则安装，再对同一安装副本运行新的安装后处理脚本，使用 `du -sk` 比较分配空间：

| 指标 | 精简前 | 精简后 |
| --- | ---: | ---: |
| `.app` 分配空间（KiB） | 149,584 | 77,000 |
| `.app` 分配空间（MiB） | 146.1 | 75.2 |

减少 72,584 KiB，约 48.5%；共裁剪 92 个二进制文件。这是安装后的体积，不是 DMG 下载大小。

新的 `dist/mqtt-plus-0.5.3-macos-arm64.dmg` 为 28,664,026 字节（约 27.3 MiB）。未测量同条件旧 DMG，不能将安装体积的降幅直接当作下载体积降幅。

验证内容：

- Release 构建及 `all_qmllint` 通过。
- 完整 CTest 46/46 通过；使用 `QT_QPA_PLATFORM=offscreen`，并允许测试访问本机回环网络。首次受限运行因窗口和回环网络不可用失败，未将其视为产品回归。
- 新增打包测试覆盖 ARM/Intel 裁剪、双架构保留、重复运行、资源及符号链接保留、缺失架构时拒绝继续。
- 精简后 `.app` 的 `codesign --verify --deep --strict` 通过。
- 清除开发环境 Qt 查找路径后，安装副本的 Lua/JavaScript 校验器均通过合法及非法输入检查。
- CPack 成功生成 DMG。首次受限生成因磁盘镜像服务不可用失败，允许访问系统服务后重试成功。

本机 QtMqtt 仍有既有的 macOS 部署目标版本警告，部署工具仍报告缺失 Mimer 客户端库；未使用的 Mimer 插件按原规则移除。此项精简不解决这些已有问题，也不代表已完成 Intel 实机、旧版 macOS、GUI 手工流程、Developer ID 签名或公证验收。

复现构建（`QT_PREFIX` 指向本机 Qt）：

```bash
rtk proxy ./scripts/package-macos.sh "${QT_PREFIX:?Set QT_PREFIX}" arm64
rtk proxy env QT_QPA_PLATFORM=offscreen ctest --test-dir build/qt6.11-debug --output-on-failure
```

隐私处理：临时安装位置和本机 Qt 路径未记录为个人绝对路径；未采集用户配置或会话数据。
