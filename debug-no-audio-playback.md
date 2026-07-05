# Debug Session: no-audio-playback
- **Status**: [OPEN]
- **Issue**: 播放视频无声音（视频进度正常）
- **Debug Server**: http://127.0.0.1:7777/event
- **Log File**: .dbg/trae-debug-log-no-audio-playback.ndjson

## Reproduction Steps
1. 启动 VideoPlayer
2. 打开任意包含音轨的视频文件
3. 观察：无声音

## Hypotheses & Verification
| ID | Hypothesis | Likelihood | Effort | Evidence |
|----|------------|------------|--------|----------|
| A | QAudioSink 进入 Stopped/Suspended 或 error!=NoError（设备/格式不支持或拉流失败） | High | Low | Pending |
| B | AudioIODevice 读不到队列数据，长期输出静音（队列没被 push 或持续 underflow） | High | Low | Pending |
| C | Swr 重采样/转换失败（swr_init/swr_convert 返回错误或输出大小为 0） | Med | Low | Pending |
| D | 实际播放文件无音轨或音轨参数异常（采样率/声道数为 0 等） | Low | Low | Pending |

## Log Evidence
(pending)

## Verification Conclusion
(pending)
