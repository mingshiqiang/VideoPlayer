#include "audio_output.h"
#include <QDebug>
#include <QAudioDevice>
#include <QMediaDevices>

AudioIODevice::AudioIODevice(AudioChunkQueue *queue, QObject *parent)
    : QIODevice(parent)
    , m_queue(queue)
{
    open(QIODevice::ReadOnly | QIODevice::Unbuffered);
}

void AudioIODevice::start()
{
    m_totalRead = 0;
}

void AudioIODevice::stop()
{
    m_buffer.clear();
}

qint64 AudioIODevice::readData(char *data, qint64 maxlen)
{
    qint64 total = 0;

    // Consume leftover from previous chunk first
    while (total < maxlen && !m_buffer.isEmpty()) {
        qint64 toCopy = qMin((qint64)m_buffer.size(), maxlen - total);
        memcpy(data + total, m_buffer.constData(), toCopy);
        m_buffer.remove(0, (int)toCopy);
        total += toCopy;
    }

    // Pull from queue
    while (total < maxlen) {
        AudioChunk chunk;
        if (!m_queue->pop(chunk, 10)) {
            break;
        }

        qint64 toCopy = qMin((qint64)chunk.data.size(), maxlen - total);
        memcpy(data + total, chunk.data.constData(), (size_t)toCopy);
        total += toCopy;

        if (toCopy < chunk.data.size()) {
            m_buffer = chunk.data.mid((int)toCopy);
        }
    }

    m_totalRead += total;

    // If we have no data, output silence to keep the stream alive
    if (total == 0) {
        memset(data, 0, (size_t)maxlen);
        return maxlen;
    }

    return total;
}

qint64 AudioIODevice::writeData(const char *data, qint64 len)
{
    Q_UNUSED(data);
    Q_UNUSED(len);
    return 0;
}

AudioOutput::AudioOutput(QObject *parent)
    : QObject(parent)
{
}

AudioOutput::~AudioOutput()
{
    stop();
}

void AudioOutput::start(int sampleRate, int channels, AudioChunkQueue *queue)
{
    stop();

    m_format.setSampleRate(sampleRate);
    m_format.setChannelCount(channels);
    m_format.setSampleFormat(QAudioFormat::Int16);

    const QAudioDevice defaultDevice = QMediaDevices::defaultAudioOutput();
    if (!defaultDevice.isFormatSupported(m_format)) {
        qWarning() << "Default audio format not supported, trying nearest";
        // Qt6 doesn't have nearestFormat; just try with Int16 which is widely supported
    }

    m_audioSink = new QAudioSink(defaultDevice, m_format, this);
    m_audioSink->setBufferSize(8192 * 4);

    m_ioDevice = new AudioIODevice(queue, this);
    m_ioDevice->start();
    m_audioSink->start(m_ioDevice);
}

void AudioOutput::stop()
{
    if (m_audioSink) {
        m_audioSink->stop();
        delete m_audioSink;
        m_audioSink = nullptr;
    }
    if (m_ioDevice) {
        m_ioDevice->stop();
        delete m_ioDevice;
        m_ioDevice = nullptr;
    }
}

void AudioOutput::setVolume(int volume)
{
    if (m_audioSink) {
        m_audioSink->setVolume(qreal(volume) / 100.0);
    }
}
