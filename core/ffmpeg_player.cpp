#include "ffmpeg_player.h"
#include <QFileInfo>
#include <QDebug>

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

    // Swresample: -> S16 stereo
    if (m_audioStreamIndex >= 0) {
        m_swrCtx = swr_alloc();
        AVChannelLayout outLayout = AV_CHANNEL_LAYOUT_STEREO;
        ret = swr_alloc_set_opts2(&m_swrCtx,
                                  &outLayout, AV_SAMPLE_FMT_S16, m_audioSampleRate,
                                  &m_audioCodecCtx->ch_layout, m_audioSampleFmt, m_audioSampleRate,
                                  0, nullptr);
        if (ret < 0) {
            qWarning() << "Failed to set swresample options";
        } else {
            ret = swr_init(m_swrCtx);
            if (ret < 0) {
                qWarning() << "Failed to init swresample";
            }
        }
        m_audioSampleFmt = AV_SAMPLE_FMT_S16;
        m_audioChannels = 2;
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
            m_audioSampleRate = codecPar->sample_rate;
            m_audioChannels = codecPar->ch_layout.nb_channels;
            m_audioSampleFmt = static_cast<AVSampleFormat>(codecPar->format);
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
}

void FFmpegPlayer::play()
{
    if (m_state == (int)PlayState::Stopped && m_fmtCtx) {
        m_stopRequested = 0;
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

void FFmpegPlayer::run()
{
    decodeLoop();
}

void FFmpegPlayer::decodeLoop()
{
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();

    while (!m_stopRequested) {
        if (m_state == (int)PlayState::Paused) {
            msleep(50);
            continue;
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

    double pts = 0.0;
    if (frame->pts != AV_NOPTS_VALUE) {
        pts = frame->pts * av_q2d(m_fmtCtx->streams[m_videoStreamIndex]->time_base);
    }

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

    // Update position
            m_positionMs.storeRelaxed((qint64)(pts * 1000));
    emit positionChanged(m_positionMs);
}

void FFmpegPlayer::processAudioFrame(AVFrame *frame)
{
    if (!m_swrCtx) return;

    double pts = 0.0;
    if (frame->pts != AV_NOPTS_VALUE) {
        pts = frame->pts * av_q2d(m_fmtCtx->streams[m_audioStreamIndex]->time_base);
    }

    // Resample to S16 stereo
    int outSamples = swr_get_out_samples(m_swrCtx, frame->nb_samples);
    if (outSamples <= 0) outSamples = frame->nb_samples * 2;

    uint8_t *outData = nullptr;
    int outLinesize = 0;
    int bufSize = av_samples_alloc(&outData, &outLinesize, 2, outSamples,
                                   AV_SAMPLE_FMT_S16, 0);
    if (bufSize < 0) return;

    int converted = swr_convert(m_swrCtx, &outData, outSamples,
                                (const uint8_t **)frame->data, frame->nb_samples);

    if (converted > 0) {
        int dataSize = av_samples_get_buffer_size(nullptr, 2, converted,
                                                   AV_SAMPLE_FMT_S16, 1);
        if (dataSize > 0) {
            AudioChunk chunk;
            chunk.data = QByteArray((const char *)outData, dataSize);
            chunk.pts = pts;
            m_audioQueue.push(chunk);

            m_audioClock = pts + (double)converted / m_audioSampleRate;
        }
    }

    av_freep(&outData);
}
