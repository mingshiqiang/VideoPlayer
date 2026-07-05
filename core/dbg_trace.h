#ifndef DBG_TRACE_H
#define DBG_TRACE_H

//
// Lightweight, dependency-free local file tracer for the no-audio-playback
// investigation. Unlike the QNetworkAccessManager-based reporter, this writes
// directly to a local file (and qDebug) so it works across threads without
// "Cannot create children for a parent that is in a different thread" issues.
//
// Output: .dbg/audio-trace.log  (one line per call, flushed on every write)
//

#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDebug>
#include <QMutex>
#include <QRecursiveMutex>
#include <QThread>

#include <atomic>

namespace dbg_trace {

inline QRecursiveMutex &mutex()
{
    static QRecursiveMutex m;
    return m;
}

// Thread-id shorthand for the log (stable, comparable within a run).
inline quintptr currentThreadId()
{
    return reinterpret_cast<quintptr>(QThread::currentThreadId());
}

inline void writeImpl(const char *location, const QString &msg)
{
    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz"));
    const QString line = QStringLiteral("[%1 tid=%2] %3 : %4")
                             .arg(ts)
                             .arg((quint64)currentThreadId(), 0, 16)
                             .arg(QString::fromUtf8(location))
                             .arg(msg);

    qDebug().noquote() << line;

    QMutexLocker locker(&mutex());
    QFile f(QStringLiteral(".dbg/audio-trace.log"));
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream s(&f);
        s << line << '\n';
        s.flush();
    }
}

// Throttled variant: log at most once per `intervalMs` per call-site, but keep
// counters accurate. Used in hot paths (readData / processAudioFrame) to avoid
// flooding the file while still showing live progress.
inline bool throttle(std::atomic<qint64> &lastMs, int intervalMs)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 last = lastMs.load(std::memory_order_relaxed);
    if (now - last < intervalMs) return true;  // throttled
    lastMs.store(now, std::memory_order_relaxed);
    return false;
}

}  // namespace dbg_trace

// Convenience macros: a per-call-site throttle counter.
#define DBG_TRACE(loc, msg) ::dbg_trace::writeImpl((loc), (msg))

#define DBG_TRACE_THROTTLE(counterName, loc, msg, intervalMs)                       \
    do {                                                                            \
        static std::atomic<qint64> counterName{0};                                  \
        if (!::dbg_trace::throttle(counterName, (intervalMs))) {                    \
            ::dbg_trace::writeImpl((loc), (msg));                                   \
        }                                                                           \
    } while (0)

#endif  // DBG_TRACE_H
