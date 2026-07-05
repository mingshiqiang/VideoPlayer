#include "mainwindow.h"
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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowFlags(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setMouseTracking(true);

    m_player = new FFmpegPlayer(this);
    m_audioOutput = new AudioOutput(this);

    setupUI();
    setupConnections();
    setupShortcuts();
    applyStylesheet();

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
    connect(m_controlBar, &ControlBar::prevClicked, this, &MainWindow::onPrevClicked);
    connect(m_controlBar, &ControlBar::nextClicked, this, &MainWindow::onNextClicked);
    connect(m_controlBar, &ControlBar::seekRequested, this, &MainWindow::onSeekRequested);
    connect(m_controlBar, &ControlBar::volumeChanged, this, &MainWindow::onVolumeChanged);
    connect(m_controlBar, &ControlBar::fullscreenToggled, this, &MainWindow::onFullscreenToggled);

    connect(m_playlistWidget, &PlaylistWidget::itemDoubleClicked, this, &MainWindow::onPlaylistItemClicked);
    connect(m_playlistWidget, &PlaylistWidget::closeRequested, this, &MainWindow::onPlaylistCloseRequested);

    connect(m_player, &FFmpegPlayer::positionChanged, this, &MainWindow::onPositionChanged);
    connect(m_player, &FFmpegPlayer::durationChanged, this, &MainWindow::onDurationChanged);
    connect(m_player, &FFmpegPlayer::stateChanged, this, &MainWindow::onStateChanged);
    connect(m_player, &FFmpegPlayer::videoInfoChanged, this, &MainWindow::onVideoInfoChanged);
    connect(m_player, &FFmpegPlayer::audioFormatChanged, this, &MainWindow::onAudioFormatChanged);
    connect(m_player, &FFmpegPlayer::finished, this, &MainWindow::onPlayerFinishedAutoNext);
    connect(m_player, &FFmpegPlayer::errorOccurred, this, &MainWindow::onPlayerError);
}

void MainWindow::setupShortcuts()
{
    auto *openShortcut = new QShortcut(QKeySequence("Ctrl+O"), this);
    connect(openShortcut, &QShortcut::activated, this, &MainWindow::onOpenFile);

    auto *spaceShortcut = new QShortcut(QKeySequence(Qt::Key_Space), this);
    connect(spaceShortcut, &QShortcut::activated, this, [this]() {
        if (m_isPlaying) onPauseClicked();
        else onPlayClicked();
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
            height: 4px;
            background: rgba(255,255,255,0.2);
            border-radius: 2px;
        }
        QSlider#seekSlider::sub-page:horizontal {
            background: #7c3aed;
            border-radius: 2px;
        }
        QSlider#seekSlider::add-page:horizontal {
            background: rgba(255,255,255,0.2);
            border-radius: 2px;
        }
        QSlider#seekSlider::handle:horizontal {
            width: 14px;
            height: 14px;
            background: #ffffff;
            border-radius: 7px;
            margin: -5px 0;
        }
        QSlider#seekSlider:hover::groove:horizontal {
            height: 6px;
            border-radius: 3px;
        }
        QSlider#seekSlider:hover::handle:horizontal {
            width: 16px;
            height: 16px;
            border-radius: 8px;
            margin: -5px 0;
        }

        QSlider#volumeSlider::groove:horizontal {
            height: 4px;
            background: rgba(255,255,255,0.2);
            border-radius: 2px;
        }
        QSlider#volumeSlider::sub-page:horizontal {
            background: rgba(255,255,255,0.7);
            border-radius: 2px;
        }
        QSlider#volumeSlider::add-page:horizontal {
            background: rgba(255,255,255,0.2);
            border-radius: 2px;
        }
        QSlider#volumeSlider::handle:horizontal {
            width: 12px;
            height: 12px;
            background: #ffffff;
            border-radius: 6px;
            margin: -4px 0;
        }
        QSlider#volumeSlider:hover::handle:horizontal {
            width: 14px;
            height: 14px;
            border-radius: 7px;
            margin: -5px 0;
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

void MainWindow::onOpenFile()
{
    QSettings settings;
    QString lastDir = settings.value("lastOpenDir").toString();
    QString path = QFileDialog::getOpenFileName(this, "Open Video File", lastDir,
        "Video Files (*.mp4 *.flv *.mkv *.avi *.mov *.webm *.ts *.wmv *.m4v);;All Files (*.*)");

    if (!path.isEmpty()) {
        settings.setValue("lastOpenDir", QFileInfo(path).absolutePath());
        m_playlistWidget->clear();
        m_playlistWidget->addFile(path);
        m_playlistWidget->setCurrentIndex(0);
        openFile(path);
    }
}

void MainWindow::onPlayClicked()
{
    if (m_player->state() == PlayState::Paused) {
        m_player->play();
        m_isPlaying = true;
        m_controlBar->setPlaying(true);
    }
}

void MainWindow::onPauseClicked()
{
    if (m_player->state() == PlayState::Playing) {
        m_player->pause();
        m_isPlaying = false;
        m_controlBar->setPlaying(false);
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
    int idx = m_playlistWidget->currentIndex();
    if (idx > 0) {
        m_playlistWidget->setCurrentIndex(idx - 1);
        openFile(m_playlistWidget->currentFilePath());
    }
}

void MainWindow::onNextClicked()
{
    int idx = m_playlistWidget->currentIndex();
    if (idx >= 0 && idx < m_playlistWidget->count() - 1) {
        m_playlistWidget->setCurrentIndex(idx + 1);
        openFile(m_playlistWidget->currentFilePath());
    } else {
        m_isPlaying = false;
        m_controlBar->setPlaying(false);
        m_controlBar->setPosition(0);
    }
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
    } else {
        showMaximized();
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

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    QMainWindow::mouseMoveEvent(event);
    if (m_isFullscreen) {
        onShowControls();
    }
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
    if (m_emptyStateLabel) {
        QString text = "Drop a video file here, or press Ctrl+O to open\n\n"
                       "Supported: MP4 / FLV / MKV / AVI / MOV / WEBM / TS";
        m_emptyStateLabel->setText(text);
        m_emptyStateLabel->adjustSize();
        m_emptyStateLabel->move(
            (m_videoCanvas->width() - m_emptyStateLabel->width()) / 2,
            (m_videoCanvas->height() - m_emptyStateLabel->height()) / 2);
    }
}
