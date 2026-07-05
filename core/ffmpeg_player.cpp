#include "ffmpeg_player.h"
#include <QFileInfo>
#include <QDebug>
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
static void dbgReportFfmpeg(const char *hypothesisId, const char *location, const QString &msg, const QJsonObject &data)
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

// Convert the atomic fixed-point speed (thousandths) to a double.
static inline double speedFromAtomic(const QAtomicInt &a)
{
    return a.loadRelaxed() / 1000.0;
}

FFmpegPlayer::FFmpegPlayer(QObject *parent)
    : QThread(parent)
    , m_videoQueue(3)
    , m_audioQueue(10)
{
}

FFmpegPlayer::~FFmpegPlayer()
{
    stop();
    if (isRunning()) {
        wait(3000);
    }
    close();
}

bool FFmpegPlayer::open(const QString &filePath)
{
    close();

    m_filePath = filePath;
    m_fileName = QFileInfo(filePath).fileName();

#pragma region debug-point D:open
    {
        QJsonObject d;
        d.insert(QStringLiteral("file"), m_filePath);
        dbgReportFfmpeg("D", "ffmpeg_player.cpp:open", "open file", d);
    }
#pragma endregion

    int ret = avformat_open_input(&m_fmtCtx, filePath.toUtf8().constData(), nullptr, nullptr);
    if (ret < 0) {
        emit errorOccurred(QString("Failed to open file: %1").arg(m_fileName));
        return false;
    }

    ret = avformat_find_stream_info(m_fmtCtx, nullptr);
    if (ret < 0) {
        emit errorOccurred("Failed to find stream info");
        close();
        return false;
    }

    if (m_fmtCtx->duration != AV_NOPTS_VALUE) {
        m_durationMs = m_fmtCtx->duration / 1000;
        emit durationChanged(m_durationMs);
    }

    if (!openStreams()) {
        close();
        return false;
    }

    // Swscale: YUV -> RGB24
    if (m_videoStreamIndex >= 0) {
        m_swsCtx = sws_getContext(
            m_videoWidth, m_videoHeight, m_videoCodecCtx->pix_fmt,
            m_videoWidth, m_videoHeight, AV_PIX_FMT_RGB24,
            SWS_BILINEAR, nullptr, nullptr, nullptr);

        int bufSize = av_image_get_buffer_size(AV_PIX_FMT_RGB24, m_videoWidth, m_videoHeight, 1);
        m_rgbBuffer = (uint8_t *)av_malloc(bufSize);
        av_image_fill_arrays(m_rgbTmpData, m_rgbTmpLinesize,
                             m_rgbBuffer, AV_PIX_FMT_RGB24, m_videoWidth, m_videoHeight, 1);
    }

    // Swresample is configured later by setAudioOutputFormat() once the
    // QAudioSink's real device format is known (some devices don't support
    // the file's native sample rate / Int16, e.g. fall back to 48000/Float).
    // Configuring swr here with hardcoded Int16 would just be rebuilt anyway.
    if (m_audioStreamIndex >= 0) {
        emit audioFormatChanged(m_audioSampleRate, m_audioChannels);
    }

    if (m_videoStreamIndex >= 0) {
        emit videoInfoChanged(m_videoWidth, m_videoHeight);
    }

    return true;
}

bool FFmpegPlayer::openStreams()
{
    for (unsigned int i = 0; i < m_fmtCtx->nb_streams; i++) {
        AVStream *stream = m_fmtCtx->streams[i];
        AVCodecParameters *codecPar = stream->codecpar;
        const AVCodec *codec = avcodec_find_decoder(codecPar->codec_id);
        if (!codec) continue;

        if (codecPar->codec_type == AVMEDIA_TYPE_VIDEO && m_videoStreamIndex < 0) {
            m_videoCodecCtx = avcodec_alloc_context3(codec);
            avcodec_parameters_to_context(m_videoCodecCtx, codecPar);
            if (avcodec_open2(m_videoCodecCtx, codec, nullptr) < 0) {
                avcodec_free_context(&m_videoCodecCtx);
                continue;
            }
            m_videoStreamIndex = i;
            m_videoWidth = codecPar->width;
            m_videoHeight = codecPar->height;
        } else if (codecPar->codec_type == AVMEDIA_TYPE_AUDIO && m_audioStreamIndex < 0) {
            m_audioCodecCtx = avcodec_alloc_context3(codec);
            avcodec_parameters_to_context(m_audioCodecCtx, codecPar);
            if (avcodec_open2(m_audioCodecCtx, codec, nullptr) < 0) {
                avcodec_free_context(&m_audioCodecCtx);
                continue;
            }
            m_audioStreamIndex = i;
            m_audioSampleRate = m_audioCodecCtx->sample_rate;
            m_audioChannels = m_audioCodecCtx->ch_layout.nb_channels;
            m_audioSampleFmt = m_audioCodecCtx->sample_fmt;
#pragma region debug-point D:audio-stream
            {
                QJsonObject d;
                d.insert(QStringLiteral("streamIndex"), (double)i);
                d.insert(QStringLiteral("codecName"), QString::fromUtf8(codec->name));
                d.insert(QStringLiteral("sampleRate"), m_audioSampleRate);
                d.insert(QStringLiteral("channels"), m_audioChannels);
                d.insert(QStringLiteral("sampleFmt"), (double)m_audioSampleFmt);
                dbgReportFfmpeg("D", "ffmpeg_player.cpp:openStreams", "audio stream opened", d);
            }
#pragma endregion
        }
    }

    if (m_videoStreamIndex < 0 && m_audioStreamIndex < 0) {
        emit errorOccurred("No video or audio stream found");
        return false;
    }

    return true;
}

void FFmpegPlayer::closeStreams()
{
    if (m_videoCodecCtx) {
        avcodec_free_context(&m_videoCodecCtx);
        m_videoCodecCtx = nullptr;
    }
    if (m_audioCodecCtx) {
        avcodec_free_context(&m_audioCodecCtx);
        m_audioCodecCtx = nullptr;
    }
    if (m_swsCtx) {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }
    if (m_swrCtx) {
        swr_free(&m_swrCtx);
        m_swrCtx = nullptr;
    }
    if (m_rgbBuffer) {
        av_free(m_rgbBuffer);
        m_rgbBuffer = nullptr;
    }
    m_videoStreamIndex = -1;
    m_audioStreamIndex = -1;
}

void FFmpegPlayer::close()
{
    stop();
    if (isRunning()) {
        wait(3000);
    }

    closeStreams();

    if (m_fmtCtx) {
        avformat_close_input(&m_fmtCtx);
        m_fmtCtx = nullptr;
    }

    m_videoQueue.clear();
    m_audioQueue.clear();
    m_durationMs = 0;
    m_positionMs.storeRelaxed(0);
    m_videoWidth = 0;
    m_videoHeight = 0;
    m_audioClock = 0.0;
    m_clockInited = 0;
    m_wallClockBaseUs = 0;
    m_ptsBaseSec = 0.0;
}

void FFmpegPlayer::play()
{
    if (m_state == (int)PlayState::Stopped && m_fmtCtx) {
        m_stopRequested = 0;
        m_clockInited = 0;
        start();
    }
    m_state = (int)PlayState::Playing;
    emit stateChanged(PlayState::Playing);
}

void FFmpegPlayer::pause()
{
    if (m_state == (int)PlayState::Playing) {
        m_state = (int)PlayState::Paused;
        emit stateChanged(PlayState::Paused);
    }
}

void FFmpegPlayer::stop()
{
    m_stopRequested = 1;
    m_state = (int)PlayState::Stopped;
    m_videoQueue.clear();
    m_audioQueue.clear();
}

void FFmpegPlayer::seek(qint64 positionMs)
{
    QMutexLocker locker(&m_seekMutex);
    m_seekTargetMs = positionMs;
    m_seekRequested = 1;
}

void FFmpegPlayer::setVolume(int volume)
{
    m_volume = volume;
}

void FFmpegPlayer::setAudioOutputFormat(int sampleRate, int channels, QAudioFormat::SampleFormat sampleFormat)
{
    if (!m_audioCodecCtx) return;

    // Remember what the sink expects; swr output rate is derived from this and
    // the current playback speed (see rebuildSwr).
    m_sinkSampleRate = sampleRate;
    m_sinkChannels = channels;
    m_sinkSampleFormat = sampleFormat;

#pragma region debug-point A:setAudioOutputFormat-entry
    {
        QJsonObject d;
        d.insert(QStringLiteral("reqSampleRate"), sampleRate);
        d.insert(QStringLiteral("reqChannels"), channels);
        d.insert(QStringLiteral("reqSampleFormat"), (double)sampleFormat);
        d.insert(QStringLiteral("inSampleRate"), m_audioCodecCtx->sample_rate);
        d.insert(QStringLiteral("inChannels"), m_audioCodecCtx->ch_layout.nb_channels);
        d.insert(QStringLiteral("inSampleFmt"), (double)m_audioCodecCtx->sample_fmt);
        dbgReportFfmpeg("A", "ffmpeg_player.cpp:setAudioOutputFormat", "setAudioOutputFormat entry", d);
    }
#pragma endregion

    rebuildSwr();
}

void FFmpegPlayer::rebuildSwr()
{
    if (!m_audioCodecCtx) return;

    if (m_swrCtx) {
        swr_free(&m_swrCtx);
    }

    const int channels = m_sinkChannels;
    const QAudioFormat::SampleFormat sampleFormat = m_sinkSampleFormat;

    AVChannelLayout outLayout;
    av_channel_layout_default(&outLayout, channels);

    AVSampleFormat outFmt = AV_SAMPLE_FMT_S16;
    switch (sampleFormat) {
    case QAudioFormat::UInt8:
        outFmt = AV_SAMPLE_FMT_U8;
        break;
    case QAudioFormat::Int16:
        outFmt = AV_SAMPLE_FMT_S16;
        break;
    case QAudioFormat::Int32:
        outFmt = AV_SAMPLE_FMT_S32;
        break;
    case QAudioFormat::Float:
        outFmt = AV_SAMPLE_FMT_FLT;
        break;
    default:
        outFmt = AV_SAMPLE_FMT_S16;
        break;
    }

    // Time-stretch via resampling: produce samples at sinkRate/speed, then the
    // sink plays them at sinkRate -> effective playback speed = `speed`.
    // Pitch shifts (this is simple resampling, not a phase-vocoder), which is
    // acceptable for a video player's quick scan mode.
    const double speed = speedFromAtomic(m_speed);
    const int outSampleRate = qBound(4000, (int)(m_sinkSampleRate / speed), 192000);

    m_swrCtx = swr_alloc();
    const int inSampleRate = m_audioCodecCtx->sample_rate;
    const AVSampleFormat inSampleFmt = m_audioCodecCtx->sample_fmt;

    int ret = swr_alloc_set_opts2(&m_swrCtx,
                                 &outLayout, outFmt, outSampleRate,
                                 &m_audioCodecCtx->ch_layout, inSampleFmt, inSampleRate,
                                 0, nullptr);
    av_channel_layout_uninit(&outLayout);
    const int initRet = (ret < 0) ? ret : swr_init(m_swrCtx);
    if (initRet < 0) {
#pragma region debug-point C:swr-init-fail
        {
            QJsonObject d;
            d.insert(QStringLiteral("allocRet"), ret);
            d.insert(QStringLiteral("initRet"), initRet);
            dbgReportFfmpeg("C", "ffmpeg_player.cpp:rebuildSwr", "swr init failed", d);
        }
#pragma endregion
        DBG_TRACE("ffmpeg_player.cpp:rebuildSwr",
                  QString("swr INIT FAILED: allocRet=%1 initRet=%2 (in=%3/%4/fmt%5 -> out=%6/%7/fmt%8 speed=%9)")
                      .arg(ret).arg(initRet)
                      .arg(inSampleRate).arg(m_audioCodecCtx->ch_layout.nb_channels).arg((int)inSampleFmt)
                      .arg(outSampleRate).arg(channels).arg((int)outFmt).arg(speed, 0, 'f', 3));
        swr_free(&m_swrCtx);
        return;
    }

    // Keep m_audioSampleRate as the SINK rate (used for clock math and chunk
    // timing); swr actually outputs at outSampleRate.
    m_audioSampleRate = m_sinkSampleRate;
    m_audioChannels = channels;
    m_audioSampleFmt = outFmt;

#pragma region debug-point C:swr-init-ok
    {
        QJsonObject d;
        d.insert(QStringLiteral("outSampleRate"), outSampleRate);
        d.insert(QStringLiteral("sinkSampleRate"), m_sinkSampleRate);
        d.insert(QStringLiteral("outChannels"), m_audioChannels);
        d.insert(QStringLiteral("outSampleFmt"), (double)m_audioSampleFmt);
        dbgReportFfmpeg("C", "ffmpeg_player.cpp:rebuildSwr", "swr init ok", d);
    }
#pragma endregion

    DBG_TRACE("ffmpeg_player.cpp:rebuildSwr",
              QString("swr OK: in=%1/%2/fmt%3 -> out=%4 (sink=%5)/%6/fmt%7 speed=%8")
                  .arg(inSampleRate).arg(m_audioCodecCtx->ch_layout.nb_channels).arg((int)inSampleFmt)
                  .arg(outSampleRate).arg(m_sinkSampleRate).arg(m_audioChannels)
                  .arg((int)m_audioSampleFmt).arg(speed, 0, 'f', 3));
}

void FFmpegPlayer::setSpeed(double speed)
{
    if (speed < 0.25) speed = 0.25;
    if (speed > 4.0) speed = 4.0;
    m_speed.storeRelaxed((int)(speed * 1000));
    // Re-create swr so audio time-stretch follows the new speed.
    rebuildSwr();
    // Reset clock so video re-syncs to the new pace immediately.
    m_clockInited.storeRelaxed(0);
    emit speedChanged(speed);
}

void FFmpegPlayer::run()
{
    decodeLoop();
}

double FFmpegPlayer::framePtsSec(const AVFrame *frame, const AVStream *stream) const
{
    int64_t ts = frame->pts;
    if (ts == AV_NOPTS_VALUE) {
        ts = frame->best_effort_timestamp;
    }
    if (ts == AV_NOPTS_VALUE) {
        return -1.0;
    }
    return ts * av_q2d(stream->time_base);
}

void FFmpegPlayer::resetClock(double ptsBaseSec)
{
    m_wallClockBaseUs = av_gettime_relative();
    m_ptsBaseSec = ptsBaseSec;
    m_clockInited.storeRelaxed(1);
}

void FFmpegPlayer::syncToClock(double ptsSec, qint64 leadUs)
{
    if (m_state != (int)PlayState::Playing) return;
    if (!m_clockInited.loadRelaxed()) {
        resetClock(ptsSec);
        return;
    }
    const double speed = speedFromAtomic(m_speed);
    const qint64 nowUs = av_gettime_relative();
    // Wall-clock target advances pts/speed: at 2x, a 1s pts span plays in 0.5s.
    const double relSec = ptsSec - m_ptsBaseSec;
    const qint64 targetUs = m_wallClockBaseUs + (qint64)(relSec / speed * 1000000.0);
    const qint64 aheadUs = targetUs - nowUs;
    if (aheadUs <= leadUs) return;
    qint64 sleepUs = aheadUs - leadUs;
    if (sleepUs > 1000000) sleepUs = 1000000;
    if (sleepUs > 0) {
        usleep((unsigned long)sleepUs);
    }
}

void FFmpegPlayer::decodeLoop()
{
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    bool wasPaused = false;
    int64_t pauseStartUs = 0;

    while (!m_stopRequested) {
        if (m_state == (int)PlayState::Paused) {
            if (!wasPaused) {
                pauseStartUs = av_gettime_relative();
                wasPaused = true;
            }
            msleep(50);
            continue;
        }
        if (wasPaused) {
            if (m_clockInited.loadRelaxed()) {
                m_wallClockBaseUs += (av_gettime_relative() - pauseStartUs);
            }
            wasPaused = false;
        }

        // Handle seek
        if (m_seekRequested) {
            int64_t targetTs = 0;
            {
                QMutexLocker locker(&m_seekMutex);
                targetTs = (int64_t)m_seekTargetMs * 1000;
                m_seekRequested = 0;
            }

            int streamIndex = m_videoStreamIndex >= 0 ? m_videoStreamIndex : m_audioStreamIndex;
            AVRational timeBase = m_fmtCtx->streams[streamIndex]->time_base;
            AVRational microSecBase = {1, 1000000};
            int64_t seekTarget = av_rescale_q(targetTs, microSecBase, timeBase);

            av_seek_frame(m_fmtCtx, streamIndex, seekTarget, AVSEEK_FLAG_BACKWARD);
            if (m_videoCodecCtx) avcodec_flush_buffers(m_videoCodecCtx);
            if (m_audioCodecCtx) avcodec_flush_buffers(m_audioCodecCtx);
            m_videoQueue.clear();
            m_audioQueue.clear();
            m_audioClock = targetTs / 1000000.0;
            resetClock(m_audioClock);
        }

        int ret = av_read_frame(m_fmtCtx, packet);
        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                // Flush decoders by sending null packet
                if (m_videoCodecCtx) {
                    avcodec_send_packet(m_videoCodecCtx, nullptr);
                    while (avcodec_receive_frame(m_videoCodecCtx, frame) == 0) {
                        processVideoFrame(frame);
                    }
                }
                if (m_audioCodecCtx) {
                    avcodec_send_packet(m_audioCodecCtx, nullptr);
                    while (avcodec_receive_frame(m_audioCodecCtx, frame) == 0) {
                        processAudioFrame(frame);
                    }
                }
                emit finished();
            }
            break;
        }

        if (packet->stream_index == m_videoStreamIndex) {
            ret = avcodec_send_packet(m_videoCodecCtx, packet);
            if (ret >= 0) {
                while (avcodec_receive_frame(m_videoCodecCtx, frame) == 0) {
                    processVideoFrame(frame);
                }
            }
        } else if (packet->stream_index == m_audioStreamIndex) {
            ret = avcodec_send_packet(m_audioCodecCtx, packet);
            if (ret >= 0) {
                while (avcodec_receive_frame(m_audioCodecCtx, frame) == 0) {
                    processAudioFrame(frame);
                }
            }
        }

        av_packet_unref(packet);
    }

    av_frame_free(&frame);
    av_packet_free(&packet);

    m_state = (int)PlayState::Stopped;
    emit stateChanged(PlayState::Stopped);
}

void FFmpegPlayer::processVideoFrame(AVFrame *frame)
{
    if (!m_swsCtx || !m_rgbBuffer) return;

    double pts = framePtsSec(frame, m_fmtCtx->streams[m_videoStreamIndex]);
    if (pts < 0.0) pts = 0.0;
    // Always pace video frames against the wall clock so the decode loop does
    // not run ahead (which would starve audio and make the progress bar race).
    // When there is audio, QAudioSink drains the audio queue at device rate;
    // when there is not, this is the only clock. Either way, video pts is the
    // correct reference for pacing the decode loop.
    syncToClock(pts, 0);

    // Convert YUV -> RGB
    sws_scale(m_swsCtx, frame->data, frame->linesize, 0, m_videoHeight,
              m_rgbTmpData, m_rgbTmpLinesize);

    // Wrap into QImage (copy data for thread safety)
    QImage img(m_rgbTmpData[0], m_videoWidth, m_videoHeight,
               m_rgbTmpLinesize[0], QImage::Format_RGB888);
    QImage copy = img.copy();

    VideoFrame vf;
    vf.image = copy;
    vf.pts = pts;

    m_videoQueue.push(vf);

    if (m_audioStreamIndex < 0) {
        m_positionMs.storeRelaxed((qint64)(pts * 1000));
        emit positionChanged(m_positionMs);
    }
}

void FFmpegPlayer::processAudioFrame(AVFrame *frame)
{
    if (!m_swrCtx) {
        DBG_TRACE_THROTTLE(paNoSwr, "ffmpeg_player.cpp:processAudioFrame",
            QStringLiteral("NO swrCtx — skipping audio frame (nb_samples=%1)").arg(frame->nb_samples),
            1000);
        return;
    }

    static qint64 lastReportMs = 0;
    static qint64 frames = 0;
    static qint64 convertedTotal = 0;
    static qint64 bytesTotal = 0;
    static std::atomic<qint64> firstFrameMs{0};

    double pts = framePtsSec(frame, m_fmtCtx->streams[m_audioStreamIndex]);
    if (pts < 0.0) {
        pts = m_audioClock;
        if (pts < 0.0) pts = 0.0;
    }
    // Audio is paced by QAudioSink pulling from the queue at device sample rate.
    // Do not block the decode thread with wall-clock sync here; pts jumps after
    // seek or non-zero start offsets would otherwise cause long usleeps and
    // starve the audio queue (sink stays Idle, outputs silence).

    // Resample to S16 stereo
    int outSamples = swr_get_out_samples(m_swrCtx, frame->nb_samples);
    if (outSamples <= 0) outSamples = frame->nb_samples * 2;

    uint8_t *outData = nullptr;
    int outLinesize = 0;
    int bufSize = av_samples_alloc(&outData, &outLinesize, m_audioChannels, outSamples,
                                   m_audioSampleFmt, 0);
    if (bufSize < 0) return;

    int converted = swr_convert(m_swrCtx, &outData, outSamples,
                                (const uint8_t **)frame->data, frame->nb_samples);

    int dataSize = 0;
    if (converted > 0) {
        dataSize = av_samples_get_buffer_size(nullptr, m_audioChannels, converted,
                                              m_audioSampleFmt, 1);
        if (dataSize > 0) {
            AudioChunk chunk;
            chunk.data = QByteArray((const char *)outData, dataSize);
            chunk.pts = pts;
            m_audioQueue.push(chunk);

            // Use the frame pts as the audio clock; the previous converted/rate
            // estimate was wrong under speed != 1 (swr outputs at sinkRate/speed).
            m_audioClock = pts;
            m_positionMs.storeRelaxed((qint64)(m_audioClock * 1000));
            emit positionChanged(m_positionMs);
        }
    }

    frames++;
    if (converted > 0) convertedTotal += converted;
    if (dataSize > 0) bytesTotal += dataSize;

    // Log the very first audio frame to confirm the decode thread reached here.
    if (firstFrameMs.load(std::memory_order_relaxed) == 0) {
        firstFrameMs.store(QDateTime::currentMSecsSinceEpoch(), std::memory_order_relaxed);
        DBG_TRACE("ffmpeg_player.cpp:processAudioFrame",
                  QString("FIRST audio frame: pts=%1 nb_samples=%2 inFmt=%3 -> converted=%4 dataSize=%5 queueSize=%6 outFmt=%7")
                      .arg(pts, 0, 'f', 3).arg(frame->nb_samples)
                      .arg((int)frame->format).arg(converted).arg(dataSize)
                      .arg(m_audioQueue.size()).arg((int)m_audioSampleFmt));
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (converted <= 0) {
#pragma region debug-point C:swr-convert-bad
        {
            QJsonObject d;
            d.insert(QStringLiteral("converted"), converted);
            d.insert(QStringLiteral("frameNbSamples"), frame->nb_samples);
            d.insert(QStringLiteral("outSamples"), outSamples);
            d.insert(QStringLiteral("pts"), pts);
            d.insert(QStringLiteral("queueSize"), (double)m_audioQueue.size());
            dbgReportFfmpeg("C", "ffmpeg_player.cpp:processAudioFrame", "swr_convert returned <=0", d);
        }
#pragma endregion
        DBG_TRACE_THROTTLE(paBadConvert, "ffmpeg_player.cpp:processAudioFrame",
            QString("swr_convert<=0: converted=%1 nb_samples=%2 outSamples=%3").arg(converted).arg(frame->nb_samples).arg(outSamples),
            500);
    } else {
        DBG_TRACE_THROTTLE(paStats, "ffmpeg_player.cpp:processAudioFrame",
            QString("audio push: frames=%1 convertedTotal=%2 bytesTotal=%3 queueSize=%4 audioClock=%5 outFmt=%6")
                .arg(frames).arg(convertedTotal).arg(bytesTotal)
                .arg(m_audioQueue.size()).arg(m_audioClock, 0, 'f', 3).arg((int)m_audioSampleFmt),
            1000);
        if (nowMs - lastReportMs >= 1000) {
#pragma region debug-point B:audio-push-stats
            {
                QJsonObject d;
                d.insert(QStringLiteral("frames"), (double)frames);
                d.insert(QStringLiteral("convertedTotal"), (double)convertedTotal);
                d.insert(QStringLiteral("bytesTotal"), (double)bytesTotal);
                d.insert(QStringLiteral("queueSize"), (double)m_audioQueue.size());
                d.insert(QStringLiteral("audioClock"), m_audioClock);
                d.insert(QStringLiteral("sampleRate"), m_audioSampleRate);
                d.insert(QStringLiteral("channels"), m_audioChannels);
                d.insert(QStringLiteral("sampleFmt"), (double)m_audioSampleFmt);
                dbgReportFfmpeg("B", "ffmpeg_player.cpp:processAudioFrame", "audio push stats", d);
            }
#pragma endregion
            lastReportMs = nowMs;
        }
    }

    av_freep(&outData);
}
