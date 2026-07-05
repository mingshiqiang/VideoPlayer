#ifndef AUDIO_OUTPUT_H
#define AUDIO_OUTPUT_H

#include <QObject>
#include <QAudioSink>
#include <QIODevice>
#include <QAudioFormat>
#include <QByteArray>

#include "video_frame_queue.h"

class AudioIODevice : public QIODevice {
    Q_OBJECT
public:
    explicit AudioIODevice(AudioChunkQueue *queue, QObject *parent = nullptr);

    void start();
    void stop();

    // Called by AudioOutput when new audio chunks are pushed, so the device
    // can emit readyRead() to wake the QAudioSink pull loop on Windows
    // (QWindowsAudioSink only re-pulls on readyRead after the initial start).
    void notifyDataReady();

protected:
    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *data, qint64 len) override;
    qint64 bytesAvailable() const override;

private:
    AudioChunkQueue *m_queue;
    QByteArray m_buffer;
    qint64 m_totalRead = 0;
};

class AudioOutput : public QObject {
    Q_OBJECT
public:
    explicit AudioOutput(QObject *parent = nullptr);
    ~AudioOutput();

    void start(int sampleRate, int channels, AudioChunkQueue *queue);
    void stop();
    void setVolume(int volume);
    const QAudioFormat& format() const { return m_format; }

private:
    QAudioSink *m_audioSink = nullptr;
    AudioIODevice *m_ioDevice = nullptr;
    QAudioFormat m_format;
};

#endif // AUDIO_OUTPUT_H
