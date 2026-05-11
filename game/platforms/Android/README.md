# minorGems Android 平台分支

本目录是 minorGems `android-port` 分支专属的 Android 平台实现，与 `SDL/`、`openGL/` 等桌面平台层并列。

## 目标

在不改动 minorGems master 分支的前提下，为 Android NDK 环境提供：

- 图形上下文（EGL + OpenGL ES 1.x，复用 Raspbian 的 GLES 代码路径）
- 音频（OpenSL ES 后端）
- 文件 I/O（APK Asset 只读 + 内部存储读写）
- 主循环胶水（配合 NativeActivity）

## 目录结构

| 路径 | 内容 |
|---|---|
| `game/platforms/Android/` | 平台入口（`gameAndroid.cpp`），替代桌面端 `SDL/gameSDL.cpp` |
| `io/file/android/` | `FileAndroid.cpp` —— AAssetManager 读取、内部存储写入 |
| `sound/android/` | `OpenSLAudioBackend.cpp` —— OpenSL ES 双缓冲 BufferQueue |

## 与其他平台层的关系

- **桌面端**（`SDL/`、`linux/`、`mac/`）：不受影响，仅通过 `master` 分支提供
- **Raspbian GLES**（`graphics/openGL/glInclude.h` 中的 `RASPBIAN` 分支）：Android 分支复用其 GLES 1.x 代码路径
- **POSIX 共用代码**（`system/unix/`、`io/file/linux/` 等）：Android NDK 兼容 POSIX，直接链接复用

## 与主仓库的关系

本分支通过 `git worktree add -b android-port` 从 master 创建。OneLife Android 项目通过 CMakeLists.txt 引用 `../../minorGems-android-port`，桌面端继续引用 `../../minorGems` 的 master 分支，两者互不干扰。
