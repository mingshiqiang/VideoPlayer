#include "controlbar.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QLinearGradient>

ControlBar::ControlBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(72);
    setAttribute(Qt::WA_TranslucentBackground);

    // -- Seek row --
    m_currentTimeLabel = new QLabel("00:00", this);
    m_currentTimeLabel->setStyleSheet(
        "color: rgba(255,255,255,0.7); font-family: 'JetBrains Mono', 'Consolas', monospace; font-size: 12px;");
    m_currentTimeLabel->setFixedWidth(48);
    m_currentTimeLabel->setAlignment(Qt::AlignCenter);

    m_seekSlider = new SeekSlider(Qt::Horizontal, this);
    m_seekSlider->setObjectName("seekSlider");

    m_totalTimeLabel = new QLabel("00:00", this);
    m_totalTimeLabel->setStyleSheet(
        "color: rgba(255,255,255,0.7); font-family: 'JetBrains Mono', 'Consolas', monospace; font-size: 12px;");
    m_totalTimeLabel->setFixedWidth(48);
    m_totalTimeLabel->setAlignment(Qt::AlignCenter);

    auto *seekLayout = new QHBoxLayout();
    seekLayout->setContentsMargins(18, 6, 18, 0);
    seekLayout->setSpacing(12);
    seekLayout->addWidget(m_currentTimeLabel);
    seekLayout->addWidget(m_seekSlider, 1);
    seekLayout->addWidget(m_totalTimeLabel);

    // -- Control row --
    m_playBtn = new QPushButton(this);
    m_playBtn->setFixedSize(44, 44);
    m_playBtn->setCursor(Qt::PointingHandCursor);
    m_playBtn->setText(">");
    m_playBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; color: white; "
        "font-size: 18px; font-weight: bold; border-radius: 22px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.1); }");

    m_prevBtn = new QPushButton(this);
    m_prevBtn->setFixedSize(38, 38);
    m_prevBtn->setCursor(Qt::PointingHandCursor);
    m_prevBtn->setText("|<");
    m_prevBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; color: rgba(255,255,255,0.7); "
        "font-size: 12px; font-weight: bold; border-radius: 19px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.1); color: white; }");

    m_nextBtn = new QPushButton(this);
    m_nextBtn->setFixedSize(38, 38);
    m_nextBtn->setCursor(Qt::PointingHandCursor);
    m_nextBtn->setText(">|");
    m_nextBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; color: rgba(255,255,255,0.7); "
        "font-size: 12px; font-weight: bold; border-radius: 19px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.1); color: white; }");

    m_volumeBtn = new QPushButton(this);
    m_volumeBtn->setFixedSize(38, 38);
    m_volumeBtn->setCursor(Qt::PointingHandCursor);
    m_volumeBtn->setText("Vol");
    m_volumeBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; color: rgba(255,255,255,0.7); "
        "font-size: 10px; font-weight: bold; border-radius: 19px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.1); color: white; }");

    m_volumeSlider = new VolumeSlider(Qt::Horizontal, this);
    m_volumeSlider->setObjectName("volumeSlider");
    m_volumeSlider->setFixedWidth(90);
    m_volumeSlider->setValue(100);

    m_fullscreenBtn = new QPushButton(this);
    m_fullscreenBtn->setFixedSize(38, 38);
    m_fullscreenBtn->setCursor(Qt::PointingHandCursor);
    m_fullscreenBtn->setText("[]");
    m_fullscreenBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; color: rgba(255,255,255,0.7); "
        "font-size: 14px; font-weight: bold; border-radius: 19px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.1); color: white; }");

    auto *leftLayout = new QHBoxLayout();
    leftLayout->setSpacing(6);
    leftLayout->addWidget(m_playBtn);
    leftLayout->addWidget(m_prevBtn);
    leftLayout->addWidget(m_nextBtn);
    leftLayout->addSpacing(8);
    leftLayout->addWidget(m_volumeBtn);
    leftLayout->addWidget(m_volumeSlider);

    auto *rightLayout = new QHBoxLayout();
    rightLayout->setSpacing(6);
    rightLayout->addWidget(m_fullscreenBtn);

    auto *controlLayout = new QHBoxLayout();
    controlLayout->setContentsMargins(18, 0, 18, 10);
    controlLayout->setSpacing(12);
    controlLayout->addLayout(leftLayout);
    controlLayout->addStretch();
    controlLayout->addLayout(rightLayout);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(4);
    mainLayout->addLayout(seekLayout);
    mainLayout->addLayout(controlLayout);

    // Connections
    connect(m_playBtn, &QPushButton::clicked, this, &ControlBar::onPlayButtonClicked);
    connect(m_prevBtn, &QPushButton::clicked, this, &ControlBar::prevClicked);
    connect(m_nextBtn, &QPushButton::clicked, this, &ControlBar::nextClicked);
    connect(m_volumeBtn, &QPushButton::clicked, this, &ControlBar::onVolumeButtonClicked);
    connect(m_fullscreenBtn, &QPushButton::clicked, this, &ControlBar::fullscreenToggled);
    connect(m_volumeSlider, &VolumeSlider::sliderMoved, this, &ControlBar::volumeChanged);

    // Seek: update position label while dragging, emit seek on release
    connect(m_seekSlider, &SeekSlider::sliderMoved, this, [this](int val) {
        if (m_durationMs > 0) {
            qint64 pos = (qint64)val * m_durationMs / 1000;
            m_currentTimeLabel->setText(formatTime(pos));
        }
    });
    connect(m_seekSlider, &SeekSlider::seekRequested, this, [this](int val) {
        if (m_durationMs > 0) {
            qint64 pos = (qint64)val * m_durationMs / 1000;
            emit seekRequested(pos);
        }
    });
}

void ControlBar::setPosition(qint64 positionMs)
{
    m_currentTimeLabel->setText(formatTime(positionMs));
    if (m_durationMs > 0 && !m_seekSlider->underMouse()) {
        QSignalBlocker blocker(m_seekSlider);
        int val = (int)(positionMs * 1000 / m_durationMs);
        m_seekSlider->setValue(val);
    }
}

void ControlBar::setDuration(qint64 durationMs)
{
    m_durationMs = durationMs;
    m_totalTimeLabel->setText(formatTime(durationMs));
}

void ControlBar::setVolume(int volume)
{
    m_volumeSlider->setValue(volume);
    m_lastVolume = volume;
    updateVolumeButton();
}

void ControlBar::setPlaying(bool playing)
{
    m_isPlaying = playing;
    updatePlayButton();
}

void ControlBar::onPlayStateChanged(bool isPlaying)
{
    setPlaying(isPlaying);
}

void ControlBar::onPlayButtonClicked()
{
    if (m_isPlaying) {
        emit pauseClicked();
    } else {
        emit playClicked();
    }
}

void ControlBar::onVolumeButtonClicked()
{
    if (m_isMuted) {
        m_isMuted = false;
        m_volumeSlider->setValue(m_lastVolume);
        emit volumeChanged(m_lastVolume);
    } else {
        m_isMuted = true;
        m_lastVolume = m_volumeSlider->value();
        m_volumeSlider->setValue(0);
        emit volumeChanged(0);
    }
    updateVolumeButton();
    emit muteToggled(m_isMuted);
}

void ControlBar::updatePlayButton()
{
    m_playBtn->setText(m_isPlaying ? "||" : ">");
}

void ControlBar::updateVolumeButton()
{
    int vol = m_volumeSlider->value();
    if (vol == 0) {
        m_volumeBtn->setText("X");
    } else if (vol < 50) {
        m_volumeBtn->setText("v");
    } else {
        m_volumeBtn->setText("V");
    }
}

QString ControlBar::formatTime(qint64 ms) const
{
    qint64 totalSec = ms / 1000;
    int hours = totalSec / 3600;
    int minutes = (totalSec % 3600) / 60;
    int seconds = totalSec % 60;

    if (hours > 0) {
        return QString::asprintf("%d:%02d:%02d", hours, minutes, seconds);
    }
    return QString::asprintf("%02d:%02d", minutes, seconds);
}

void ControlBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.fillRect(rect(), QColor(20, 20, 24, 225));
    p.setPen(QColor(255, 255, 255, 15));
    p.drawLine(0, 0, width(), 0);
}
