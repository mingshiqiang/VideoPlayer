#ifndef FFMPEG_PLAYER_H
#define FFMPEG_PLAYER_H

#include <QThread>
#include <QImage>
#include <QString>
#include <QMutex>
#include <QAtomicInt>
#include <QAtomicInteger>
#include <QWaitCondition>
#include <QAudioFormat>

#include "video_frame_queue.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <libavutil/channel_layout.h>
}

enum class PlayState {
    Stopped,
    Playing,
    Paused
};

class FFmpegPlayer : public QThread {
    Q_OBJECT

public:
    explicit FFmpegPlayer(QObject *parent = nullptr);
    ~FFmpegPlayer() override;

    bool open(const QString &filePath);
    void close();
    void play();
    void pause();
    void stop();
    void seek(qint64 positionMs);
    void setVolume(int volume);
    void setAudioOutputFormat(int sampleRate, int channels, QAudioFormat::SampleFormat sampleFormat);

    PlayState state() const { return (PlayState)m_state.loadRelaxed(); }
    qint64 duration() const { return m_durationMs; }
    qint64 position() const { return m_positionMs.loadRelaxed(); }
    int volume() const { return m_volume; }
    bool hasVideo() const { return m_videoStreamIndex >= 0; }
    bool hasAudio() const { return m_audioStreamIndex >= 0; }
    QString fileName() const { return m_fileName; }
    int videoWidth() const { return m_videoWidth; }
    int videoHeight() const { return m_videoHeight; }

    VideoFrameQueue* videoQueue() { return &m_videoQueue; }
    AudioChunkQueue* audioQueue() { return &m_audioQueue; }

    int audioSampleRate() const { return m_audioSampleRate; }
    int audioChannels() const { return m_audioChannels; }
    AVSampleFormat audioSampleFormat() const { return AV_SAMPLE_FMT_S16; }

signals:
    void stateChanged(PlayState state);
    void positionChanged(qint64 positionMs);
    void durationChanged(qint64 durationMs);
    void videoInfoChanged(int width, int height);
    void audioFormatChanged(int sampleRate, int channels);
    void errorOccurred(const QString &message);
    void finished();

protected:
    void run() override;

private:
    bool openStreams();
    void closeStreams();
    void decodeLoop();
    void processVideoFrame(AVFrame *frame);
    void processAudioFrame(AVFrame *frame);
    double framePtsSec(const AVFrame *frame, const AVStream *stream) const;
    void resetClock(double ptsBaseSec);
    void syncToClock(double ptsSec, qint64 leadUs);

    // FFmpeg context
    AVFormatContext *m_fmtCtx = nullptr;
    AVCodecContext  *m_videoCodecCtx = nullptr;
    AVCodecContext  *m_audioCodecCtx = nullptr;
    SwsContext      *m_swsCtx = nullptr;
    SwrContext      *m_swrCtx = nullptr;

    int m_videoStreamIndex = -1;
    int m_audioStreamIndex = -1;

    // Video info
    int m_videoWidth = 0;
    int m_videoHeight = 0;

    // RGB conversion buffer
    uint8_t *m_rgbBuffer = nullptr;
    uint8_t *m_rgbTmpData[4] = {nullptr};
    int m_rgbTmpLinesize[4] = {0};

    // Audio info
    int m_audioSampleRate = 44100;
    int m_audioChannels = 2;
    AVSampleFormat m_audioSampleFmt = AV_SAMPLE_FMT_S16;

    // State
    QAtomicInt m_state { (int)PlayState::Stopped };
    QAtomicInt m_seekRequested { 0 };
    QAtomicInt m_seekTargetMs { 0 };
    QAtomicInt m_volume { 100 };
    QAtomicInt m_stopRequested { 0 };

    qint64 m_durationMs = 0;
    QAtomicInteger<qint64> m_positionMs { 0 };
    QString m_filePath;
    QString m_fileName;

    // Clock
    double m_audioClock = 0.0;
    qint64 m_wallClockBaseUs = 0;
    double m_ptsBaseSec = 0.0;
    QAtomicInt m_clockInited { 0 };

    // Frame queues
    VideoFrameQueue m_videoQueue;
    AudioChunkQueue m_audioQueue;

    // Sync
    QMutex m_seekMutex;
};

#endif // FFMPEG_PLAYER_H
