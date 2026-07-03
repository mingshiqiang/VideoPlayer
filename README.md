# VideoPlayer

基于 **C++17 / Qt6 / FFmpeg 8.1** 的跨平台视频播放器。采用 CMake 构建，使用 OpenGL 渲染视频帧、QAudioSink 输出音频，UI 为无窗框自定义样式，支持播放列表、拖拽打开、全屏、进度拖动等常用功能。

---

## 目录

- [功能特性](#功能特性)
- [项目结构](#项目结构)
- [依赖环境](#依赖环境)
- [构建与运行](#构建与运行)
- [使用说明](#使用说明)
- [键盘快捷键](#键盘快捷键)
- [技术实现](#技术实现)
- [许可证](#许可证)

---

## 功能特性

- **本地视频播放**：支持 MP4、FLV、MKV、AVI、MOV、WEBM、TS、WMV、M4V 等常见格式。
- **音视频解码**：基于 FFmpeg 8.1 进行解封装、视频解码（YUV → RGB）、音频重采样（S16 立体声）。
- **OpenGL 渲染**：`VideoCanvas` 使用 OpenGL 3.3 Core Profile 纹理渲染视频帧，刷新率约 60 FPS。
- **Qt 音频输出**：通过 `QAudioSink` + 自定义 `QIODevice` 实时消费解码后的 PCM 数据。
- **无窗框界面**：自定义标题栏、控制栏，支持窗口拖动、最大化/还原、双击标题栏切换。
- **播放列表**：可显示/隐藏，支持双击切换、上一曲/下一曲、自动播放下一集。
- **进度与音量控制**：可拖动进度条跳转、调整音量、一键静音。
- **全屏模式**：支持全屏播放，鼠标移动时自动显示控制栏，3 秒后自动隐藏。
- **拖拽打开**：支持拖拽一个或多个视频文件到播放窗口。
- **快捷键**：空格播放/暂停、方向键快进/快退、F 全屏、M 静音等。
- **窗口状态记忆**：退出时自动保存窗口几何，下次启动恢复。

---

## 项目结构

```text
VideoPlayer/
├── CMakeLists.txt              # CMake 构建配置
├── main.cpp                    # 程序入口
├── mainwindow.h / .cpp / .ui   # 主窗口（无窗框、组合各 UI 模块）
├── core/                       # 播放器核心
│   ├── ffmpeg_player.h/.cpp    # FFmpeg 解码线程
│   ├── audio_output.h/.cpp     # Qt 音频输出
│   └── video_frame_queue.h     # 线程安全视频/音频队列
├── ui/                         # 自定义 UI 组件
│   ├── titlebar.h/.cpp         # 标题栏（拖拽、最大化、打开文件）
│   ├── controlbar.h/.cpp       # 底部控制栏（播放、进度、音量、全屏）
│   ├── video_canvas.h/.cpp     # OpenGL 视频渲染画布
│   ├── slider.h/.cpp           # 自定义进度条/音量条
│   └── playlist_widget.h/.cpp  # 播放列表
└── third_part/
    └── ffmpeg-8.1-full_build-shared/   # 预编译 FFmpeg 8.1 共享库
        ├── include/            # FFmpeg 头文件
        ├── lib/                # 导入库 (.lib)
        └── bin/                # 运行时 DLL
```

---

## 依赖环境

| 依赖 | 版本要求 | 说明 |
|------|----------|------|
| C++ 标准 | C++17 | CMake 中已强制启用 |
| Qt | 6.5+ | 需安装 `Core`、`Widgets`、`Multimedia`、`OpenGL`、`OpenGLWidgets` 模块 |
| FFmpeg | 8.1 | 项目已内置 `third_part/ffmpeg-8.1-full_build-shared` |
| CMake | 3.19+ | 构建工具 |
| 编译器 | MSVC 2022 / MinGW / Clang | 当前工程默认使用 MSVC2022 64-bit |
| 显卡驱动 | 支持 OpenGL 3.3 Core | 用于视频渲染 |

> **提示**：当前 FFmpeg 为 Windows 64-bit 共享库。若要在 Linux/macOS 上构建，请自行替换为对应平台的 FFmpeg 动态/静态库，并调整 `CMakeLists.txt` 中的库路径与链接方式。

---

## 构建与运行

### 1. 环境准备

- 安装 Qt 6.5 或更高版本（推荐通过 [Qt Online Installer](https://www.qt.io/download-qt-installer) 安装）。
- 安装 CMake（3.19+）。
- 确保 Qt 的 `bin` 目录已加入系统 `PATH`，或在 IDE 中配置好 Qt 工具链。

### 2. 使用 Qt Creator / VS Code 构建

```bash
# 克隆或进入项目目录
cd VideoPlayer

# 创建构建目录
mkdir build && cd build

# 配置（根据你的 Qt 安装路径调整 CMAKE_PREFIX_PATH）
cmake .. -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64

# 编译
cmake --build . --config Release
```

### 3. 运行

构建完成后，Windows 下 FFmpeg 的 DLL 会通过 CMake 的 `POST_BUILD` 命令自动复制到输出目录，因此直接运行生成的 `VideoPlayer.exe` 即可。

```bash
# 例如
./Release/VideoPlayer.exe
```

---

## 使用说明

1. **打开文件**：点击标题栏 `+` 按钮，或按 `Ctrl + O` 选择视频文件。
2. **播放控制**：使用底部控制栏的播放/暂停、上一曲、下一曲按钮。
3. **拖动进度**：鼠标按住进度条拖动，松开后跳转到对应位置。
4. **调整音量**：拖动音量滑块，或点击音量图标静音/恢复。
5. **全屏播放**：点击全屏按钮或按 `F`，移动鼠标显示控制栏，3 秒后自动隐藏；按 `Esc` 退出全屏。
6. **播放列表**：点击标题栏 `=` 按钮显示/隐藏列表，双击列表项切换视频，支持拖拽多个文件到窗口追加到播放列表。

---

## 键盘快捷键

| 快捷键 | 功能 |
|--------|------|
| `Ctrl + O` | 打开视频文件 |
| `Space` | 播放 / 暂停 |
| `←` | 后退 5 秒 |
| `→` | 前进 5 秒 |
| `↑` | 音量 +10 |
| `↓` | 音量 -10 |
| `M` | 静音 / 恢复 |
| `F` | 切换全屏 |
| `Esc` | 退出全屏 |
| `L` | 显示 / 隐藏播放列表 |

---

## 技术实现

### 1. FFmpeg 解码流程

`FFmpegPlayer` 继承自 `QThread`，在独立线程中执行解码循环：

1. `avformat_open_input` / `avformat_find_stream_info` 打开文件并探测流信息。
2. 分别打开第一个视频流和音频流的 `AVCodecContext`。
3. 视频通过 `libswscale` 将解码后的 YUV 帧转换为 `RGB24`，再包装为 `QImage`。
4. 音频通过 `libswresample` 重采样为 `S16` 立体声 PCM，封装为 `AudioChunk`。
5. 解码后的视频帧压入 `VideoFrameQueue`，音频块压入 `AudioChunkQueue`，由 UI 和音频输出线程消费。
6. 支持 `av_seek_frame` 实现拖动跳转，并在跳转时清空队列、刷新解码器缓冲区。

### 2. 视频渲染

`VideoCanvas` 继承 `QOpenGLWidget`，使用简单的全屏四边形 + 纹理方式渲染每一帧：

- 顶点/片段着色器将 RGBA 纹理贴到全屏四边形。
- 通过 `QTimer` 以约 60 FPS 从 `VideoFrameQueue` 取出最新帧并上传为 `QOpenGLTexture`。
- 当队列积压时，`VideoFrameQueue` 会自动丢弃最旧的帧，保持低延迟。

### 3. 音频输出

`AudioOutput` 使用 Qt6 的 `QAudioSink` 配合自定义 `AudioIODevice`：

- `AudioIODevice::readData` 中从 `AudioChunkQueue` 弹出 PCM 数据返回给音频后端。
- 队列为空时输出静音，防止音频流中断产生爆音。
- 音量通过 `QAudioSink::setVolume` 控制。

### 4. UI 架构

- `MainWindow`：无窗框主窗口，负责组合 `TitleBar`、`VideoCanvas`、`PlaylistWidget`、`ControlBar`。
- 所有 UI 信号通过 Qt 信号槽连接到底层播放器，保持界面与解码线程分离。
- 样式通过 `setStyleSheet` 集中管理，整体采用深色主题。

---

## 许可证

本项目采用 MIT 许可证开源，详见 [LICENSE](LICENSE) 文件。

> 注意：项目内置的 FFmpeg 二进制文件遵循其自身的开源许可证（LGPL/GPL，视编译选项而定）。若用于商业发布，请确认所使用的 FFmpeg 构建版本符合相关许可要求。

---

## 致谢

- [Qt Project](https://www.qt.io/)
- [FFmpeg](https://ffmpeg.org/)
