#ifndef CONTROLBAR_H
#define CONTROLBAR_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include "slider.h"

class QMenu;
class VolumePopup;
class VolumePopup;
class VolumePopup;

class ControlBar : public QWidget {
    Q_OBJECT
public:
    explicit ControlBar(QWidget *parent = nullptr);

    void setPosition(qint64 positionMs);
    void setDuration(qint64 durationMs);
    void setVolume(int volume);
    void setPlaying(bool playing);
    void setSeekSliderRange(qint64 durationMs);
    void setSpeed(double speed);

    SeekSlider* seekSlider() { return m_seekSlider; }
    VolumeSlider* volumeSlider() { return m_volumeSlider; }

signals:
    void playClicked();
    void pauseClicked();
    void togglePlayRequested();
    void prevClicked();
    void nextClicked();
    void seekRequested(qint64 positionMs);
    void volumeChanged(int volume);
    void muteToggled(bool muted);
    void fullscreenToggled();
    void speedChanged(double speed);
    void screenshotRequested();
    void settingsRequested();

public slots:
    void onPlayStateChanged(bool isPlaying);

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onPlayButtonClicked();
    void onVolumeButtonClicked();
    void onSpeedButtonClicked();
    void onSpeedMenuTriggered(QAction *action);

private:
    QLabel *m_currentTimeLabel;
    SeekSlider *m_seekSlider;
    QLabel *m_totalTimeLabel;

    QPushButton *m_playBtn;
    QPushButton *m_prevBtn;
    QPushButton *m_nextBtn;
    QPushButton *m_volumeBtn;
    VolumeSlider *m_volumeSlider;
    QWidget *m_volumePopup = nullptr;
    QPushButton *m_fullscreenBtn;
    QPushButton *m_speedBtn;
    QPushButton *m_screenshotBtn;
    QPushButton *m_settingsBtn;
    QMenu *m_speedMenu = nullptr;

    bool m_isPlaying = false;
    bool m_isMuted = false;
    int m_lastVolume = 100;
    qint64 m_durationMs = 0;
    int m_speedIndex = 2;   // index into kSpeedPresets (default 1.0x)

    QString formatTime(qint64 ms) const;
    void updatePlayButton();
    void updateVolumeButton();
    void updateSpeedButton();
};

#endif // CONTROLBAR_H
