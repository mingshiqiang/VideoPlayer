#include "audio_output.h"
#include <QDebug>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QUrl>
#include <QThread>

#include "dbg_trace.h"

#pragma region debug-point no-audio-playback:reporter
static void dbgReportAudio(const char *hypothesisId, const char *location, const QString &msg, const QJsonObject &data)
{
    static QString url;
    static QString sessionId;
    static QNetworkAccessManager *nam = nullptr;
    static bool inited = false;

    if (!inited) {
        url = QStringLiteral("http://127.0.0.1:7777/event");
        sessionId = QStringLiteral("no-audio-playback");
        QFile f(QStringLiteral(".dbg/no-audio-playback.env"));
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QByteArray c = f.readAll();
            const QList<QByteArray> lines = c.split('\n');
            for (const QByteArray &line : lines) {
                if (line.startsWith("DEBUG_SERVER_URL=")) url = QString::fromUtf8(line.mid(strlen("DEBUG_SERVER_URL=")).trimmed());
                if (line.startsWith("DEBUG_SESSION_ID=")) sessionId = QString::fromUtf8(line.mid(strlen("DEBUG_SESSION_ID=")).trimmed());
            }
        }
        nam = new QNetworkAccessManager();
        inited = true;
    }

    if (!nam) return;

    QJsonObject obj;
    obj.insert(QStringLiteral("sessionId"), sessionId);
    obj.insert(QStringLiteral("runId"), QStringLiteral("pre"));
    obj.insert(QStringLiteral("hypothesisId"), QString::fromUtf8(hypothesisId));
    obj.insert(QStringLiteral("location"), QString::fromUtf8(location));
    obj.insert(QStringLiteral("msg"), QStringLiteral("[DEBUG] ") + msg);
    obj.insert(QStringLiteral("ts"), QDateTime::currentMSecsSinceEpoch());
    obj.insert(QStringLiteral("data"), data);

    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    nam->post(req, QJsonDocument(obj).toJson(QJsonDocument::Compact));
}
#pragma endregion

AudioIODevice::AudioIODevice(AudioChunkQueue *queue, QObject *parent)
    : QIODevice(parent)
    , m_queue(queue)
{
    // Must be opened ReadOnly before being passed to QAudioSink::start(); the
    // sink checks isReadable() and only pulls from readable devices.
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

// On Windows, QWindowsAudioSink::pullSource() gates every pull on
// bytesAvailable(): it computes readLen = qMin(bytesFree(), bytesAvailable())
// and only calls read() (which invokes readData()) when readLen > 0. The
// QIODevice default bytesAvailable() returns size()-pos, which is 0 for a
// custom device that doesn't override size(). That makes the sink never pull,
// stay Idle, and never recover — see QTBUG-108672.
//
// We are a streaming source fed by a thread-safe queue, so report that we can
// always deliver data. readData() then fills as much as it can and zero-pads
// the rest. Returning a large positive value is the documented workaround for
// generator/streaming devices (see Qt's audiooutput example Generator class).
qint64 AudioIODevice::bytesAvailable() const
{
    return m_buffer.size() + 4 * 1024 * 1024;  // leftover + "stream has more"
}

void AudioIODevice::notifyDataReady()
{
    // Wake the sink's pull loop. QWindowsAudioSink connects readyRead to
    // pullSource, so emitting here re-triggers a pull after the queue is fed.
    emit readyRead();
}

qint64 AudioIODevice::readData(char *data, qint64 maxlen)
{
    qint64 total = 0;
    static qint64 readCalls = 0;
    static qint64 silenceCalls = 0;
    static qint64 lastReportMs = 0;
    static std::atomic<qint64> firstReadMs{0};

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

    // Log the very first readData call regardless of throttle, so we can tell
    // whether QAudioSink ever pulls from our device.
    if (firstReadMs.load(std::memory_order_relaxed) == 0) {
        firstReadMs.store(QDateTime::currentMSecsSinceEpoch(), std::memory_order_relaxed);
        DBG_TRACE("audio_output.cpp:readData",
                  QString("FIRST readData call: maxlen=%1 provided=%2 queueSize=%3")
                      .arg(maxlen).arg(total).arg(m_queue->size()));
    }

    if (total < maxlen) {
        memset(data + total, 0, (size_t)(maxlen - total));
        silenceCalls++;
        // Throttled log for the silence/underflow path (sink still pulling but
        // queue empty). This is the key signal for "no audio" diagnosis.
        DBG_TRACE_THROTTLE(rdSilence, "audio_output.cpp:readData",
            QString("SILENCE maxlen=%1 provided=%2 silenceCalls=%3 queueSize=%4")
                .arg(maxlen).arg(total).arg(silenceCalls).arg(m_queue->size()),
            500);
        return maxlen;
    }
    readCalls++;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (nowMs - lastReportMs >= 1000) {
        QJsonObject d;
        d.insert(QStringLiteral("readCalls"), (double)readCalls);
        d.insert(QStringLiteral("silenceCalls"), (double)silenceCalls);
        d.insert(QStringLiteral("queueSize"), (double)m_queue->size());
        d.insert(QStringLiteral("maxlen"), (double)maxlen);
        d.insert(QStringLiteral("provided"), (double)total);
        dbgReportAudio("B", "audio_output.cpp:readData", "AudioIODevice readData stats", d);
        lastReportMs = nowMs;
    }

    return maxlen;
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

    const QAudioDevice defaultDevice = QMediaDevices::defaultAudioOutput();
    QAudioFormat desired;
    desired.setSampleRate(sampleRate);
    desired.setChannelCount(channels);
    desired.setSampleFormat(QAudioFormat::Int16);

    const bool desiredSupported = defaultDevice.isFormatSupported(desired);
    if (defaultDevice.isFormatSupported(desired)) {
        m_format = desired;
    } else {
        QAudioFormat preferred = defaultDevice.preferredFormat();
        QAudioFormat tryInt16 = preferred;
        tryInt16.setSampleFormat(QAudioFormat::Int16);
        m_format = defaultDevice.isFormatSupported(tryInt16) ? tryInt16 : preferred;
    }

    m_audioSink = new QAudioSink(defaultDevice, m_format, this);
    m_audioSink->setBufferSize(8192 * 4);
    connect(m_audioSink, &QAudioSink::stateChanged, this, [this](QAudio::State st) {
        QJsonObject d;
        d.insert(QStringLiteral("state"), (double)st);
        d.insert(QStringLiteral("error"), (double)m_audioSink->error());
        d.insert(QStringLiteral("bytesFree"), (double)m_audioSink->bytesFree());
        d.insert(QStringLiteral("processedUSecs"), (double)m_audioSink->processedUSecs());
        dbgReportAudio("A", "audio_output.cpp:stateChanged", "QAudioSink stateChanged", d);

        DBG_TRACE("audio_output.cpp:stateChanged",
                  QString("state=%1 error=%2 bytesFree=%3 processedUSecs=%4")
                      .arg((int)st).arg((int)m_audioSink->error())
                      .arg(m_audioSink->bytesFree()).arg(m_audioSink->processedUSecs()));
    });

    m_ioDevice = new AudioIODevice(queue, this);
    m_ioDevice->start();
    m_audioSink->start(m_ioDevice);

    QJsonObject d;
    d.insert(QStringLiteral("deviceId"), QString::fromUtf8(defaultDevice.id()));
    d.insert(QStringLiteral("deviceDescription"), defaultDevice.description());
    d.insert(QStringLiteral("desiredSupported"), desiredSupported);
    d.insert(QStringLiteral("desiredSampleRate"), desired.sampleRate());
    d.insert(QStringLiteral("desiredChannels"), desired.channelCount());
    d.insert(QStringLiteral("desiredSampleFormat"), (double)desired.sampleFormat());
    d.insert(QStringLiteral("finalSampleRate"), m_format.sampleRate());
    d.insert(QStringLiteral("finalChannels"), m_format.channelCount());
    d.insert(QStringLiteral("finalSampleFormat"), (double)m_format.sampleFormat());
    dbgReportAudio("A", "audio_output.cpp:start", "AudioOutput started", d);

    DBG_TRACE("audio_output.cpp:start",
              QString("AudioOutput started: desired=%1/%2/fmt%3 supported=%4 -> final=%5/%6/fmt%7 (device=%8)")
                  .arg(desired.sampleRate()).arg(desired.channelCount()).arg((int)desired.sampleFormat())
                  .arg(desiredSupported)
                  .arg(m_format.sampleRate()).arg(m_format.channelCount()).arg((int)m_format.sampleFormat())
                  .arg(defaultDevice.description()));
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
