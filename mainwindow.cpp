#include "mainwindow.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFile>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QDebug>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QSettings>
#include <QMessageBox>
#include <QStatusBar>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>
#include <QImageWriter>
#include <QPainter>
#include <QCursor>

namespace {
void enableMouseTrackingForTree(QWidget *widget)
{
    if (!widget) return;

    widget->setMouseTracking(true);
    const auto children = widget->findChildren<QWidget *>();
    for (QWidget *child : children) {
        child->setMouseTracking(true);
    }
}

Qt::CursorShape cursorForResizeRegion(int region)
{
    switch (region) {
    case 11:
    case 33:
        return Qt::SizeFDiagCursor;
    case 13:
    case 31:
        return Qt::SizeBDiagCursor;
    case 21:
    case 23:
        return Qt::SizeHorCursor;
    case 12:
    case 32:
        return Qt::SizeVerCursor;
    default:
        return Qt::ArrowCursor;
    }
}
}  // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowMinMaxButtonsHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setMouseTracking(true);

    m_player = new FFmpegPlayer(this);
    m_audioOutput = new AudioOutput(this);

    setupUI();
    enableMouseTrackingForTree(this);
    setupConnections();
    setupShortcuts();
    applyStylesheet();

    // Capture mouse movement anywhere (even over child widgets) so we can
    // reveal the controls again when the cursor moves while fullscreen.
    qApp->installEventFilter(this);

    m_hideTimer = new QTimer(this);
    m_hideTimer->setSingleShot(true);
    m_hideTimer->setInterval(3000);
    connect(m_hideTimer, &QTimer::timeout, this, &MainWindow::onHideControls);

    // Control bar opacity effect for fade
    m_controlBarEffect = new QGraphicsOpacityEffect(m_controlBar);
    m_controlBarEffect->setOpacity(1.0);
    m_controlBar->setGraphicsEffect(m_controlBarEffect);

    m_titleBarEffect = new QGraphicsOpacityEffect(m_titleBar);
    m_titleBarEffect->setOpacity(1.0);
    m_titleBar->setGraphicsEffect(m_titleBarEffect);

    // Restore window geometry
    QSettings settings;
    restoreGeometry(settings.value("geometry").toByteArray());
    if (!settings.contains("geometry")) {
        resize(1100, 680);
    }
    setMinimumSize(640, 400);

    // Take keyboard focus on the window itself so global shortcuts (Space,
    // F, L, arrows, ...) are captured and no child button shows a focus rect.
    setFocusPolicy(Qt::StrongFocus);
    setFocus();

    // The video canvas gets its final layout size after show(); center the
    // empty-state label once the event loop has applied the startup geometry.
    QTimer::singleShot(0, this, [this]() {
        updateEmptyStateGeometry();
    });
}

MainWindow::~MainWindow()
{
    m_player->stop();
    if (m_player->isRunning()) {
        m_player->wait(3000);
    }
    QSettings settings;
    settings.setValue("geometry", saveGeometry());
}

void MainWindow::setupUI()
{
    m_centralWidget = new QWidget(this);
    m_centralWidget->setObjectName("centralWidget");

    m_titleBar = new TitleBar(m_centralWidget);

    m_contentWidget = new QWidget(m_centralWidget);
    auto *contentLayout = new QHBoxLayout(m_contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    m_videoCanvas = new VideoCanvas(m_contentWidget);
    m_videoCanvas->setMouseTracking(true);
    contentLayout->addWidget(m_videoCanvas, 1);

    m_playlistWidget = new PlaylistWidget(m_contentWidget);
    contentLayout->addWidget(m_playlistWidget);

    // Empty state label overlay
    m_emptyStateLabel = new QLabel(m_videoCanvas);
    m_emptyStateLabel->setObjectName("emptyState");
    m_emptyStateLabel->setAlignment(Qt::AlignCenter);
    m_emptyStateLabel->setMouseTracking(true);
    m_emptyStateLabel->setText("Drop a video file here, or press Ctrl+O to open\n\n"
                               "Supported: MP4 / FLV / MKV / AVI / MOV / WEBM / TS");
    m_emptyStateLabel->adjustSize();

    m_controlBar = new ControlBar(m_centralWidget);

    auto *layout = new QVBoxLayout(m_centralWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_titleBar);
    layout->addWidget(m_contentWidget, 1);
    layout->addWidget(m_controlBar);

    setCentralWidget(m_centralWidget);
    setAcceptDrops(true);
    setMouseTracking(true);
}

void MainWindow::setupConnections()
{
    connect(m_titleBar, &TitleBar::openFileClicked, this, &MainWindow::onOpenFile);
    connect(m_titleBar, &TitleBar::minimizeClicked, this, &QWidget::showMinimized);
    connect(m_titleBar, &TitleBar::maximizeClicked, this, &MainWindow::onMaximizeClicked);
    connect(m_titleBar, &TitleBar::closeClicked, this, &QWidget::close);
    connect(m_titleBar, &TitleBar::togglePlaylistClicked, this, &MainWindow::onTogglePlaylist);

    connect(m_controlBar, &ControlBar::playClicked, this, &MainWindow::onPlayClicked);
    connect(m_controlBar, &ControlBar::pauseClicked, this, &MainWindow::onPauseClicked);
    connect(m_controlBar, &ControlBar::togglePlayRequested, this, &MainWindow::onTogglePlay);
    connect(m_controlBar, &ControlBar::prevClicked, this, &MainWindow::onPrevClicked);
    connect(m_controlBar, &ControlBar::nextClicked, this, &MainWindow::onNextClicked);
    connect(m_controlBar, &ControlBar::seekRequested, this, &MainWindow::onSeekRequested);
    connect(m_controlBar, &ControlBar::volumeChanged, this, &MainWindow::onVolumeChanged);
    connect(m_controlBar, &ControlBar::fullscreenToggled, this, &MainWindow::onFullscreenToggled);
    connect(m_controlBar, &ControlBar::speedChanged, this, &MainWindow::onSpeedChanged);
    connect(m_controlBar, &ControlBar::screenshotRequested, this, &MainWindow::onScreenshot);
    connect(m_controlBar, &ControlBar::settingsRequested, this, &MainWindow::onSettings);

    connect(m_playlistWidget, &PlaylistWidget::itemDoubleClicked, this, &MainWindow::onPlaylistItemClicked);
    connect(m_playlistWidget, &PlaylistWidget::closeRequested, this, &MainWindow::onPlaylistCloseRequested);

    connect(m_player, &FFmpegPlayer::positionChanged, this, &MainWindow::onPositionChanged);
    connect(m_player, &FFmpegPlayer::durationChanged, this, &MainWindow::onDurationChanged);
    connect(m_player, &FFmpegPlayer::stateChanged, this, &MainWindow::onStateChanged);
    connect(m_player, &FFmpegPlayer::videoInfoChanged, this, &MainWindow::onVideoInfoChanged);
    connect(m_player, &FFmpegPlayer::audioFormatChanged, this, &MainWindow::onAudioFormatChanged);
    connect(m_player, &FFmpegPlayer::speedChanged, this, [this](double s){ m_controlBar->setSpeed(s); });
    connect(m_player, &FFmpegPlayer::finished, this, &MainWindow::onPlayerFinishedAutoNext);
    connect(m_player, &FFmpegPlayer::errorOccurred, this, &MainWindow::onPlayerError);
}

void MainWindow::setupShortcuts()
{
    auto *openShortcut = new QShortcut(QKeySequence("Ctrl+O"), this);
    connect(openShortcut, &QShortcut::activated, this, &MainWindow::onOpenFile);

    auto *spaceShortcut = new QShortcut(QKeySequence(Qt::Key_Space), this);
    connect(spaceShortcut, &QShortcut::activated, this, [this]() {
        onTogglePlay();
    });

    auto *fsShortcut = new QShortcut(QKeySequence("F"), this);
    connect(fsShortcut, &QShortcut::activated, this, &MainWindow::onFullscreenToggled);

    auto *escShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escShortcut, &QShortcut::activated, this, [this]() {
        if (m_isFullscreen) onFullscreenToggled();
    });

    auto *leftShortcut = new QShortcut(QKeySequence(Qt::Key_Left), this);
    connect(leftShortcut, &QShortcut::activated, this, [this]() {
        qint64 pos = m_player->position() - 5000;
        if (pos < 0) pos = 0;
        onSeekRequested(pos);
    });

    auto *rightShortcut = new QShortcut(QKeySequence(Qt::Key_Right), this);
    connect(rightShortcut, &QShortcut::activated, this, [this]() {
        qint64 pos = m_player->position() + 5000;
        qint64 dur = m_player->duration();
        if (pos > dur) pos = dur;
        onSeekRequested(pos);
    });

    auto *upShortcut = new QShortcut(QKeySequence(Qt::Key_Up), this);
    connect(upShortcut, &QShortcut::activated, this, [this]() {
        int vol = m_player->volume() + 10;
        if (vol > 100) vol = 100;
        onVolumeChanged(vol);
    });

    auto *downShortcut = new QShortcut(QKeySequence(Qt::Key_Down), this);
    connect(downShortcut, &QShortcut::activated, this, [this]() {
        int vol = m_player->volume() - 10;
        if (vol < 0) vol = 0;
        onVolumeChanged(vol);
    });

    auto *muteShortcut = new QShortcut(QKeySequence("M"), this);
    connect(muteShortcut, &QShortcut::activated, this, [this]() {
        static bool muted = false;
        muted = !muted;
        if (muted) {
            m_lastVolume = m_player->volume();
            onVolumeChanged(0);
        } else {
            onVolumeChanged(m_lastVolume > 0 ? m_lastVolume : 100);
        }
    });

    auto *playlistShortcut = new QShortcut(QKeySequence("L"), this);
    connect(playlistShortcut, &QShortcut::activated, this, &MainWindow::onTogglePlaylist);
}

void MainWindow::applyStylesheet()
{
    setStyleSheet(R"(
        QMainWindow { background-color: #0a0a0c; }
        #centralWidget { background-color: #0a0a0c; }
        QLabel { color: white; font-family: "Microsoft YaHei UI", "Segoe UI", sans-serif; }

        QLabel#emptyState {
            color: rgba(255,255,255,0.4);
            font-size: 15px;
            font-weight: 500;
            padding: 40px;
        }

        QSlider#seekSlider::groove:horizontal {
            height: 6px;
            background: rgba(255,255,255,0.2);
            border-radius: 3px;
        }
        QSlider#seekSlider::sub-page:horizontal {
            background: #7c3aed;
            border-radius: 3px;
        }
        QSlider#seekSlider::add-page:horizontal {
            background: rgba(255,255,255,0.2);
            border-radius: 3px;
        }
        QSlider#seekSlider::handle:horizontal {
            width: 16px;
            height: 16px;
            background: #ffffff;
            border-radius: 8px;
            margin: -6px 0;
        }
        QSlider#seekSlider:hover::groove:horizontal {
            height: 8px;
            border-radius: 4px;
        }
        QSlider#seekSlider:hover::handle:horizontal {
            width: 18px;
            height: 18px;
            border-radius: 9px;
            margin: -6px 0;
        }

        QSlider#volumeSlider::groove:horizontal {
            height: 6px;
            background: rgba(255,255,255,0.2);
            border-radius: 3px;
        }
        QSlider#volumeSlider::sub-page:horizontal {
            background: rgba(255,255,255,0.7);
            border-radius: 3px;
        }
        QSlider#volumeSlider::add-page:horizontal {
            background: rgba(255,255,255,0.2);
            border-radius: 3px;
        }
        QSlider#volumeSlider::handle:horizontal {
            width: 14px;
            height: 14px;
            background: #ffffff;
            border-radius: 7px;
            margin: -5px 0;
        }
        QSlider#volumeSlider:hover::groove:horizontal {
            height: 8px;
            border-radius: 4px;
        }
        QSlider#volumeSlider:hover::handle:horizontal {
            width: 16px;
            height: 16px;
            border-radius: 8px;
            margin: -6px 0;
        }

        QListWidget {
            background-color: transparent;
            border: none;
            outline: none;
            color: white;
            font-size: 13px;
        }
        QListWidget::item {
            padding: 8px 12px;
            border-bottom: 1px solid rgba(255,255,255,0.04);
        }
        QListWidget::item:hover {
            background-color: rgba(255,255,255,0.08);
        }
        QListWidget::item:selected {
            background-color: rgba(124,58,237,0.15);
        }

        QScrollBar:vertical {
            background: transparent;
            width: 6px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: rgba(255,255,255,0.2);
            border-radius: 3px;
            min-height: 30px;
        }
        QScrollBar::handle:vertical:hover {
            background: rgba(255,255,255,0.35);
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: transparent;
        }

        QToolTip {
            background-color: rgba(20,20,24,0.95);
            color: white;
            border: 1px solid rgba(255,255,255,0.1);
            border-radius: 6px;
            padding: 4px 8px;
            font-size: 12px;
        }

        QStatusBar { background: transparent; }
        QStatusBar::item { border: none; }
        QStatusBar QLabel { color: rgba(255,255,255,0.85); font-size: 12px; padding: 2px 8px; }
    )");
}

void MainWindow::openFile(const QString &path)
{
    if (m_player->state() != PlayState::Stopped) {
        m_player->stop();
        if (m_player->isRunning()) m_player->wait(3000);
    }

    m_audioOutput->stop();
    m_audioStarted = false;

    if (!m_player->open(path)) {
        return;
    }

    m_titleBar->setFileName(m_player->fileName());
    m_emptyStateLabel->hide();

    m_videoCanvas->setVideoFrameQueue(m_player->videoQueue());

    if (m_player->hasAudio()) {
        m_audioOutput->start(m_player->audioSampleRate(), m_player->audioChannels(),
                             m_player->audioQueue());
        const QAudioFormat &fmt = m_audioOutput->format();
        m_player->setAudioOutputFormat(fmt.sampleRate(), fmt.channelCount(), fmt.sampleFormat());
        m_audioOutput->setVolume(m_player->volume());
        m_audioStarted = true;
    }

    m_player->play();
    m_isPlaying = true;
    m_controlBar->setPlaying(true);
    m_controlBar->setDuration(m_player->duration());

    m_playlistWidget->setDurationForCurrent(m_player->duration());
}

void MainWindow::updateEmptyStateGeometry()
{
    if (!m_emptyStateLabel || !m_videoCanvas) {
        return;
    }

    m_emptyStateLabel->adjustSize();
    m_emptyStateLabel->move(
        (m_videoCanvas->width() - m_emptyStateLabel->width()) / 2,
        (m_videoCanvas->height() - m_emptyStateLabel->height()) / 2);
}

void MainWindow::onOpenFile()
{
    QSettings settings;
    QString lastDir = settings.value("lastOpenDir").toString();
    QStringList paths = QFileDialog::getOpenFileNames(this, "Open Video Files", lastDir,
        "Video Files (*.mp4 *.flv *.mkv *.avi *.mov *.webm *.ts *.wmv *.m4v);;All Files (*.*)");

    if (paths.isEmpty()) return;

    settings.setValue("lastOpenDir", QFileInfo(paths.first()).absolutePath());

    // Replace the playlist with the newly selected files and start from the first.
    m_playlistWidget->clear();
    m_playlistWidget->addFiles(paths);
    m_playlistWidget->setCurrentIndex(0);
    openFile(paths.first());
}

void MainWindow::onPlayClicked()
{
    if (m_player->state() == PlayState::Paused) {
        m_player->play();
        if (m_audioStarted) m_audioOutput->resume();
        m_isPlaying = true;
        m_controlBar->setPlaying(true);
    }
}

void MainWindow::onPauseClicked()
{
    if (m_player->state() == PlayState::Playing) {
        m_player->pause();
        if (m_audioStarted) m_audioOutput->suspend();
        m_isPlaying = false;
        m_controlBar->setPlaying(false);
    }
}

void MainWindow::onTogglePlay()
{
    // Single entry point for the play/pause button: act on the real player state
    // rather than the ControlBar's cached flag, which can lag behind during a
    // file switch (async stateChanged signals reorder).
    switch (m_player->state()) {
    case PlayState::Playing:
        onPauseClicked();
        break;
    case PlayState::Paused:
        onPlayClicked();
        break;
    case PlayState::Stopped:
        // Nothing to toggle when fully stopped (no file / finished).
        break;
    }
}

void MainWindow::onSeekRequested(qint64 positionMs)
{
    m_player->seek(positionMs);
    m_controlBar->setPosition(positionMs);
}

void MainWindow::onVolumeChanged(int volume)
{
    m_player->setVolume(volume);
    m_audioOutput->setVolume(volume);
    m_controlBar->setVolume(volume);
}

void MainWindow::onPositionChanged(qint64 positionMs)
{
    m_controlBar->setPosition(positionMs);
}

void MainWindow::onDurationChanged(qint64 durationMs)
{
    m_controlBar->setDuration(durationMs);
}

void MainWindow::onStateChanged(PlayState state)
{
    m_isPlaying = (state == PlayState::Playing);
    m_controlBar->setPlaying(m_isPlaying);
}

void MainWindow::onVideoInfoChanged(int width, int height)
{
    qDebug() << "Video:" << width << "x" << height;
}

void MainWindow::onAudioFormatChanged(int sampleRate, int channels)
{
    qDebug() << "Audio:" << sampleRate << "Hz," << channels << "channels";
}

void MainWindow::onSpeedChanged(double speed)
{
    m_player->setSpeed(speed);
}

void MainWindow::onScreenshot()
{
    QImage frame = m_videoCanvas->currentFrame();
    if (frame.isNull()) {
        statusBar()->showMessage("No video frame to capture", 2500);
        return;
    }

    QString dir = SettingsDialog::screenshotDir();
    QString fmt = SettingsDialog::screenshotFormat();   // "png" | "jpg"

    QDir().mkpath(dir);

    QString base = m_player->fileName();
    if (base.isEmpty()) base = "screenshot";
    base = QFileInfo(base).completeBaseName();

    QString ts = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString fileName = QString("%1_%2.%3").arg(base, ts, fmt);
    QString path = QDir(dir).filePath(fileName);

    // QImageWriter picks format from extension; handle quality for jpg.
    QImageWriter writer(path, fmt.toLatin1());
    if (fmt == "jpg") {
        writer.setQuality(92);
        // JPG has no alpha channel; flatten onto black.
        QImage flat(frame.size(), QImage::Format_RGB32);
        flat.fill(Qt::black);
        QPainter p(&flat);
        p.drawImage(0, 0, frame.convertToFormat(QImage::Format_RGB32));
        p.end();
        if (!writer.write(flat)) {
            statusBar()->showMessage("Failed to save screenshot", 3000);
            return;
        }
    } else {
        if (!writer.write(frame)) {
            statusBar()->showMessage("Failed to save screenshot", 3000);
            return;
        }
    }

    statusBar()->showMessage(QString("Saved: %1").arg(QDir::toNativeSeparators(path)), 4000);
}

void MainWindow::onSettings()
{
    SettingsDialog dlg(this);
    dlg.exec();
}

void MainWindow::onPlayerFinishedAutoNext()
{
    if (m_playlistWidget->count() > 1) {
        onNextClicked();
    } else {
        m_isPlaying = false;
        m_controlBar->setPlaying(false);
        m_controlBar->setPosition(0);
    }
}

void MainWindow::onPlayerError(const QString &msg)
{
    QMessageBox::warning(this, "Playback Error", msg);
}

void MainWindow::onPrevClicked()
{
    if (m_playlistWidget->count() == 0) return;
    int idx = m_playlistWidget->currentIndex();
    // Wrap around: from the first item go to the last.
    int prev = (idx <= 0) ? m_playlistWidget->count() - 1 : idx - 1;
    m_playlistWidget->setCurrentIndex(prev);
    openFile(m_playlistWidget->currentFilePath());
}

void MainWindow::onNextClicked()
{
    if (m_playlistWidget->count() == 0) return;
    int idx = m_playlistWidget->currentIndex();
    // Wrap around: from the last item go to the first.
    int next = (idx < 0 || idx >= m_playlistWidget->count() - 1) ? 0 : idx + 1;
    m_playlistWidget->setCurrentIndex(next);
    openFile(m_playlistWidget->currentFilePath());
}

void MainWindow::onPlaylistItemClicked(int index)
{
    m_playlistWidget->setCurrentIndex(index);
    openFile(m_playlistWidget->filePathAt(index));
}

void MainWindow::onTogglePlaylist()
{
    m_playlistVisible = !m_playlistVisible;
    m_playlistWidget->setPlaylistVisible(m_playlistVisible);
}

void MainWindow::onPlaylistCloseRequested()
{
    m_playlistVisible = false;
    m_playlistWidget->setPlaylistVisible(false);
}

void MainWindow::onFullscreenToggled()
{
    if (m_isFullscreen) {
        showNormal();
        m_titleBar->show();
        if (m_playlistVisible) m_playlistWidget->show();
        m_isFullscreen = false;
        m_controlBar->setVisible(true);
        m_controlBarEffect->setOpacity(1.0);
        m_titleBarEffect->setOpacity(1.0);
        setCursor(Qt::ArrowCursor);
    } else {
        clearResizeCursor();
        showFullScreen();
        m_titleBar->hide();
        m_playlistWidget->hide();
        m_isFullscreen = true;
        m_hideTimer->start();
    }
}

void MainWindow::onMaximizeClicked()
{
    if (isMaximized()) {
        showNormal();
        m_titleBar->setMaximized(false);
    } else {
        clearResizeCursor();
        showMaximized();
        m_titleBar->setMaximized(true);
    }
}

void MainWindow::onHideControls()
{
    if (m_isFullscreen && m_isPlaying) {
        auto *anim1 = new QPropertyAnimation(m_controlBarEffect, "opacity");
        anim1->setDuration(300);
        anim1->setStartValue(1.0);
        anim1->setEndValue(0.0);
        anim1->start(QAbstractAnimation::DeleteWhenStopped);

        auto *anim2 = new QPropertyAnimation(m_titleBarEffect, "opacity");
        anim2->setDuration(300);
        anim2->setStartValue(1.0);
        anim2->setEndValue(0.0);
        anim2->start(QAbstractAnimation::DeleteWhenStopped);

        m_controlsVisible = false;
        setCursor(Qt::BlankCursor);
    }
}

void MainWindow::onShowControls()
{
    if (m_isFullscreen) {
        auto *anim1 = new QPropertyAnimation(m_controlBarEffect, "opacity");
        anim1->setDuration(200);
        anim1->setStartValue(m_controlBarEffect->opacity());
        anim1->setEndValue(1.0);
        anim1->start(QAbstractAnimation::DeleteWhenStopped);

        auto *anim2 = new QPropertyAnimation(m_titleBarEffect, "opacity");
        anim2->setDuration(200);
        anim2->setStartValue(m_titleBarEffect->opacity());
        anim2->setEndValue(1.0);
        anim2->start(QAbstractAnimation::DeleteWhenStopped);

        m_controlsVisible = true;
        setCursor(Qt::ArrowCursor);
        m_hideTimer->start();
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    QMainWindow::keyPressEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        QList<QUrl> urls = mimeData->urls();
        QStringList paths;
        for (const QUrl &url : urls) {
            QString path = url.toLocalFile();
            if (!path.isEmpty()) {
                paths << path;
            }
        }
        if (!paths.isEmpty()) {
            if (m_playlistWidget->count() == 0) {
                m_playlistWidget->addFiles(paths);
                m_playlistWidget->setCurrentIndex(0);
                openFile(paths.first());
            } else {
                m_playlistWidget->addFiles(paths);
            }
        }
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    updateEmptyStateGeometry();
    QTimer::singleShot(0, this, [this]() {
        updateEmptyStateGeometry();
    });
}

// --- Frameless window edge resizing (qtframeless-style 3×3 grid) -----------
//
// Adapted from the qtframeless project's approach: a 3×3 grid hit-test on
// mouse events that identifies which edge/corner the cursor is on, then
// resizes the window by computing a delta from the mouse position and applying
// it to the appropriate edge(s) of geometry() via setGeometry.
//
// Because child widgets consume mouse events, we install an event filter at
// the application level (qApp) so that every MouseMove / MouseButtonPress /
// MouseButtonRelease reaches us regardless of which child is under the cursor.

static const int kResizeEdge = 5; // Same edge width used by qtframeless.

int MainWindow::hitTest(const QPoint &pos) const
{
    if (m_isFullscreen || isMaximized()) return RegionNone;

    const int w = width();
    const int h = height();
    const int x = pos.x();
    const int y = pos.y();

    int rx = 0, ry = 0;  // 1=left/top, 2=middle, 3=right/bottom

    // Horizontal zone
    if (x <= kResizeEdge)           rx = 1;
    else if (x >= w - kResizeEdge)  rx = 3;
    else                            rx = 2;

    // Vertical zone
    if (y <= kResizeEdge)           ry = 1;
    else if (y >= h - kResizeEdge)  ry = 3;
    else                            ry = 2;

    // If the cursor is in the title bar (not on its top edge), it's a drag
    // zone — we let the TitleBar's own mouse handling take care of dragging.
    const int titleBarHeight = m_titleBar ? m_titleBar->height() : 40;
    if (ry == 2 && y < titleBarHeight && rx == 2)
        return RegionNone;  // let TitleBar handle dragging

    // Center of the window (not near any edge) — not a resize zone.
    if (rx == 2 && ry == 2)
        return RegionNone;

    return ry * 10 + rx;  // 11=LT, 12=T, 13=RT, 21=L, 22=M, 23=R, 31=LB, 32=B, 33=RB
}

void MainWindow::updateCursorForRegion(int region)
{
    const Qt::CursorShape shape = cursorForResizeRegion(region);
    if (shape == Qt::ArrowCursor) {
        clearResizeCursor();
        return;
    }

    if (m_resizeCursorOverridden) {
        QApplication::changeOverrideCursor(QCursor(shape));
    } else {
        QApplication::setOverrideCursor(QCursor(shape));
        m_resizeCursorOverridden = true;
    }
    setCursor(shape);
}

void MainWindow::clearResizeCursor()
{
    if (m_resizeCursorOverridden) {
        QApplication::restoreOverrideCursor();
        m_resizeCursorOverridden = false;
    }
    if (!m_isFullscreen) {
        unsetCursor();
    }
}

void MainWindow::startResize(int region, const QPoint &globalPos)
{
    m_resizing = true;
    m_resizeRegion = region;
    m_resizeStartGlobal = globalPos;
    m_resizeStartPos = pos();
    m_resizeStartGeo = geometry();
    updateCursorForRegion(region);
    grabMouse();
}

void MainWindow::doResize(const QPoint &globalPos)
{
    if (!m_resizing) return;

    const QPoint delta = globalPos - m_resizeStartGlobal;
    QRect g = m_resizeStartGeo;

    const int minW = qMax(minimumWidth(), 640);
    const int minH = qMax(minimumHeight(), 400);

    switch (m_resizeRegion) {
    case RegionLeftTop:
        g.setTopLeft(g.topLeft() + delta);
        if (g.width() < minW) g.setLeft(g.right() - minW + 1);
        if (g.height() < minH) g.setTop(g.bottom() - minH + 1);
        break;
    case RegionTop:
        g.setTop(g.top() + delta.y());
        if (g.height() < minH) g.setTop(g.bottom() - minH + 1);
        break;
    case RegionRightTop:
        g.setTopRight(g.topRight() + delta);
        if (g.width() < minW) g.setRight(g.left() + minW - 1);
        if (g.height() < minH) g.setTop(g.bottom() - minH + 1);
        break;
    case RegionLeft:
        g.setLeft(g.left() + delta.x());
        if (g.width() < minW) g.setLeft(g.right() - minW + 1);
        break;
    case RegionRight:
        g.setRight(g.right() + delta.x());
        if (g.width() < minW) g.setRight(g.left() + minW - 1);
        break;
    case RegionLeftBottom:
        g.setBottomLeft(g.bottomLeft() + delta);
        if (g.width() < minW) g.setLeft(g.right() - minW + 1);
        if (g.height() < minH) g.setBottom(g.top() + minH - 1);
        break;
    case RegionBottom:
        g.setBottom(g.bottom() + delta.y());
        if (g.height() < minH) g.setBottom(g.top() + minH - 1);
        break;
    case RegionRightBottom:
        g.setBottomRight(g.bottomRight() + delta);
        if (g.width() < minW) g.setRight(g.left() + minW - 1);
        if (g.height() < minH) g.setBottom(g.top() + minH - 1);
        break;
    default:
        return;
    }

    setGeometry(g);
}

void MainWindow::endResize()
{
    if (mouseGrabber() == this) {
        releaseMouse();
    }
    m_resizing = false;
    m_resizeRegion = RegionNone;
    clearResizeCursor();
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !m_isFullscreen) {
        int region = hitTest(event->pos());
        if (region != RegionNone) {
            startResize(region, event->globalPosition().toPoint());
            event->accept();
            return;
        }
    }
    QMainWindow::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_resizing) {
        doResize(event->globalPosition().toPoint());
        event->accept();
        return;
    }
    QMainWindow::mouseMoveEvent(event);
    if (m_isFullscreen) {
        onShowControls();
        return;
    }
    // Cursor preview on edges.
    int region = hitTest(event->pos());
    updateCursorForRegion(region);
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_resizing) {
        endResize();
        event->accept();
        return;
    }
    QMainWindow::mouseReleaseEvent(event);
}

void MainWindow::leaveEvent(QEvent *event)
{
    if (!m_resizing && !m_isFullscreen)
        clearResizeCursor();
    QMainWindow::leaveEvent(event);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    // Intercept mouse events from child widgets so that edge resize works
    // even when the cursor is over a child (video canvas, control bar, etc.).
    if (!m_isFullscreen && event->isInputEvent()) {
        QWidget *src = qobject_cast<QWidget *>(obj);
        if (!src || src->window() != this) {
            return QMainWindow::eventFilter(obj, event);
        }

        switch (event->type()) {
        case QEvent::MouseMove: {
            auto *me = static_cast<QMouseEvent *>(event);
            if (m_resizing) {
                doResize(me->globalPosition().toPoint());
                return true;
            }
            // Map the event position from the child to the main window.
            QPoint localPos = mapFromGlobal(me->globalPosition().toPoint());
            int region = hitTest(localPos);
            updateCursorForRegion(region);
            break;
        }
        case QEvent::MouseButtonPress: {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                QPoint localPos = mapFromGlobal(me->globalPosition().toPoint());
                int region = hitTest(localPos);
                if (region != RegionNone) {
                    startResize(region, me->globalPosition().toPoint());
                    return true;
                }
            }
            break;
        }
        case QEvent::MouseButtonRelease: {
            if (m_resizing) {
                endResize();
                return true;
            }
            break;
        }
        default:
            break;
        }
    }

    // Reveal controls on any mouse move while fullscreen.
    if (m_isFullscreen && event->type() == QEvent::MouseMove)
        onShowControls();

    return QMainWindow::eventFilter(obj, event);
}
