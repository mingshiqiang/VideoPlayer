#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileDialog>
#include <QShortcut>
#include <QTimer>
#include <QLabel>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QGraphicsOpacityEffect>

#include "core/ffmpeg_player.h"
#include "core/audio_output.h"
#include "ui/titlebar.h"
#include "ui/controlbar.h"
#include "ui/video_canvas.h"
#include "ui/playlist_widget.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onOpenFile();
    void onPlayClicked();
    void onPauseClicked();
    void onSeekRequested(qint64 positionMs);
    void onVolumeChanged(int volume);
    void onPositionChanged(qint64 positionMs);
    void onDurationChanged(qint64 durationMs);
    void onStateChanged(PlayState state);
    void onVideoInfoChanged(int width, int height);
void onAudioFormatChanged(int sampleRate, int channels);
void onFullscreenToggled();
    void onMaximizeClicked();
    void onHideControls();
    void onShowControls();
    void onPrevClicked();
    void onNextClicked();
    void onPlaylistItemClicked(int index);
    void onTogglePlaylist();
    void onPlaylistCloseRequested();
    void onPlayerFinishedAutoNext();
    void onPlayerError(const QString &msg);

private:
    void setupUI();
    void setupConnections();
    void setupShortcuts();
    void applyStylesheet();
    void openFile(const QString &path);

    FFmpegPlayer *m_player;
    AudioOutput *m_audioOutput;

    QWidget *m_centralWidget;
    TitleBar *m_titleBar;
    VideoCanvas *m_videoCanvas;
    ControlBar *m_controlBar;
    PlaylistWidget *m_playlistWidget;
    QWidget *m_contentWidget;
    QLabel *m_emptyStateLabel;

    QTimer *m_hideTimer;
    bool m_controlsVisible = true;
    bool m_isFullscreen = false;

    bool m_isPlaying = false;
    bool m_audioStarted = false;
    bool m_playlistVisible = true;
    int m_lastVolume = 100;
    QGraphicsOpacityEffect *m_controlBarEffect = nullptr;
    QGraphicsOpacityEffect *m_titleBarEffect = nullptr;
};

#endif // MAINWINDOW_H
