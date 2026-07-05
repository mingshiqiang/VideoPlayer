#ifndef VIDEO_FRAME_QUEUE_H
#define VIDEO_FRAME_QUEUE_H

#include <QImage>
#include <QMutex>
#include <QQueue>
#include <QWaitCondition>
#include <QByteArray>
#include <QMutex>

// Video frame data with timestamp
struct VideoFrame {
    QImage image;
    double pts = 0.0;  // presentation timestamp in seconds
};

// Audio PCM data chunk
struct AudioChunk {
    QByteArray data;
    double pts = 0.0;
};

// Thread-safe video frame queue (max 3 frames, drops oldest when full)
class VideoFrameQueue {
public:
    VideoFrameQueue(int maxSize = 3) : m_maxSize(maxSize) {}

    void push(const VideoFrame &frame) {
        QMutexLocker locker(&m_mutex);
        while (m_queue.size() >= m_maxSize) {
            m_queue.dequeue();  // drop oldest frame to prevent backlog
        }
        m_queue.enqueue(frame);
        m_cond.wakeOne();
    }

    bool pop(VideoFrame &frame, unsigned long timeout = 50) {
        QMutexLocker locker(&m_mutex);
        if (m_queue.isEmpty()) {
            m_cond.wait(&m_mutex, timeout);
        }
        if (m_queue.isEmpty()) return false;
        frame = m_queue.dequeue();
        return true;
    }

    void clear() {
        QMutexLocker locker(&m_mutex);
        m_queue.clear();
    }

    bool isEmpty() {
        QMutexLocker locker(&m_mutex);
        return m_queue.isEmpty();
    }

    int size() {
        QMutexLocker locker(&m_mutex);
        return m_queue.size();
    }

private:
    QQueue<VideoFrame> m_queue;
    QMutex m_mutex;
    QWaitCondition m_cond;
    int m_maxSize;
};

// Thread-safe audio chunk queue
class AudioChunkQueue {
public:
    AudioChunkQueue(int maxSize = 10) : m_maxSize(maxSize) {}

    void push(const AudioChunk &chunk) {
        QMutexLocker locker(&m_mutex);
        while (m_queue.size() >= m_maxSize) {
            m_queue.dequeue();
        }
        m_queue.enqueue(chunk);
        m_cond.wakeOne();
    }

    bool pop(AudioChunk &chunk, unsigned long timeout = 50) {
        QMutexLocker locker(&m_mutex);
        if (m_queue.isEmpty()) {
            m_cond.wait(&m_mutex, timeout);
        }
        if (m_queue.isEmpty()) return false;
        chunk = m_queue.dequeue();
        return true;
    }

    void clear() {
        QMutexLocker locker(&m_mutex);
        m_queue.clear();
    }

    int size() {
        QMutexLocker locker(&m_mutex);
        return m_queue.size();
    }

private:
    QQueue<AudioChunk> m_queue;
    QMutex m_mutex;
    QWaitCondition m_cond;
    int m_maxSize;
};

#endif // VIDEO_FRAME_QUEUE_H
