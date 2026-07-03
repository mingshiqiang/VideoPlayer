#ifndef CONTROLBAR_H
#define CONTROLBAR_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include "slider.h"

class ControlBar : public QWidget {
    Q_OBJECT
public:
    explicit ControlBar(QWidget *parent = nullptr);

    void setPosition(qint64 positionMs);
    void setDuration(qint64 durationMs);
    void setVolume(int volume);
    void setPlaying(bool playing);
    void setSeekSliderRange(qint64 durationMs);

    SeekSlider* seekSlider() { return m_seekSlider; }
    VolumeSlider* volumeSlider() { return m_volumeSlider; }

signals:
    void playClicked();
    void pauseClicked();
    void prevClicked();
    void nextClicked();
    void seekRequested(qint64 positionMs);
    void volumeChanged(int volume);
    void muteToggled(bool muted);
    void fullscreenToggled();

public slots:
    void onPlayStateChanged(bool isPlaying);

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onPlayButtonClicked();
    void onVolumeButtonClicked();

private:
    QLabel *m_currentTimeLabel;
    SeekSlider *m_seekSlider;
    QLabel *m_totalTimeLabel;

    QPushButton *m_playBtn;
    QPushButton *m_prevBtn;
    QPushButton *m_nextBtn;
    QPushButton *m_volumeBtn;
    VolumeSlider *m_volumeSlider;
    QPushButton *m_fullscreenBtn;

    bool m_isPlaying = false;
    bool m_isMuted = false;
    int m_lastVolume = 100;
    qint64 m_durationMs = 0;

    QString formatTime(qint64 ms) const;
    void updatePlayButton();
    void updateVolumeButton();
};

#endif // CONTROLBAR_H
