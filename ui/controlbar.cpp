#include "controlbar.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QPen>
#include <QLinearGradient>
#include <QIcon>
#include <QMenu>
#include <QAction>
#include <QApplication>

namespace {
QIcon makeIcon(const char *resPath)
{
    return QIcon(QString::fromUtf8(resPath));
}

// Playback speed presets; clicking the speed button cycles through these.
const double kSpeedPresets[] = { 0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 3.0 };
const int kSpeedPresetCount = sizeof(kSpeedPresets) / sizeof(kSpeedPresets[0]);

QString speedLabel(double s)
{
    // 1x / 2x / 3x for integers, otherwise 0.5x / 0.75x / 1.25x / 1.5x
    if (qFuzzyCompare(s, qRound(s)))
        return QString::number((int)qRound(s)) + QStringLiteral("x");
    return QString::number(s, 'f', 2) + QStringLiteral("x");
}
}  // namespace

// A small frameless popup hosting a vertical volume slider, shown above the
// volume button. Closes when it loses focus / the user clicks elsewhere.
class VolumePopup : public QWidget {
public:
    explicit VolumePopup(QWidget *parent = nullptr)
        : QWidget(parent, Qt::Popup)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setFixedSize(44, 150);

        m_slider = new VolumeSlider(Qt::Vertical, this);
        m_slider->setRange(0, 100);
        m_slider->setValue(100);
        m_slider->setFixedSize(28, 120);
        m_slider->setStyleSheet(
            "QSlider::groove:vertical { width: 6px; background: rgba(255,255,255,0.2); border-radius: 3px; }"
            "QSlider::sub-page:vertical { background: #7c3aed; border-radius: 3px; }"
            "QSlider::handle:vertical { height: 14px; width: 14px; background: white; border-radius: 7px; margin: -4px -4px; }"
            "QSlider::handle:vertical:hover { background: #e9d5ff; }");

        auto *lay = new QVBoxLayout(this);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->addWidget(m_slider, 0, Qt::AlignCenter);
    }

    void setValue(int v)
    {
        QSignalBlocker b(m_slider);
        m_slider->setValue(v);
    }

    int value() const { return m_slider->value(); }

    VolumeSlider *slider() const { return m_slider; }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(QColor(255, 255, 255, 30), 1));
        p.setBrush(QColor(20, 20, 24, 235));
        p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 8, 8);
    }

private:
    VolumeSlider *m_slider = nullptr;
};

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
    m_playBtn->setIcon(makeIcon(":/icons/resources/play.svg"));
    m_playBtn->setIconSize(QSize(22, 22));
    m_playBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 22px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.1); }");

    m_prevBtn = new QPushButton(this);
    m_prevBtn->setFixedSize(38, 38);
    m_prevBtn->setCursor(Qt::PointingHandCursor);
    m_prevBtn->setIcon(makeIcon(":/icons/resources/previous.svg"));
    m_prevBtn->setIconSize(QSize(18, 18));
    m_prevBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 19px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.1); }");

    m_nextBtn = new QPushButton(this);
    m_nextBtn->setFixedSize(38, 38);
    m_nextBtn->setCursor(Qt::PointingHandCursor);
    m_nextBtn->setIcon(makeIcon(":/icons/resources/next.svg"));
    m_nextBtn->setIconSize(QSize(18, 18));
    m_nextBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 19px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.1); }");

    m_volumeBtn = new QPushButton(this);
    m_volumeBtn->setFixedSize(38, 38);
    m_volumeBtn->setCursor(Qt::PointingHandCursor);
    m_volumeBtn->setToolTip("Volume");
    m_volumeBtn->setIcon(makeIcon(":/icons/resources/volume.svg"));
    m_volumeBtn->setIconSize(QSize(18, 18));
    m_volumeBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 19px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.1); }");

    // Hidden horizontal slider kept only as the value model; the real
    // control is the vertical popup shown above the volume button.
    m_volumeSlider = new VolumeSlider(Qt::Horizontal, this);
    m_volumeSlider->setObjectName("volumeSlider");
    m_volumeSlider->setVisible(false);
    m_volumeSlider->setValue(100);

    // Vertical volume popup (created on first show).
    auto *vp = new VolumePopup(this);
    m_volumePopup = vp;
    vp->hide();
    connect(vp->slider(), &VolumeSlider::sliderMoved, this, &ControlBar::volumeChanged);
    connect(vp->slider(), &QSlider::valueChanged, this, [this, vp](int v) {
        // valueChanged fires on programmatic + user changes; only forward user
        // changes to avoid loops. sliderMoved already handles drag; this covers
        // wheel/key changes inside the popup.
        if (vp->slider()->isSliderDown() || v != m_volumeSlider->value()) {
            m_volumeSlider->setValue(v);
            m_lastVolume = (v > 0) ? v : m_lastVolume;
            updateVolumeButton();
            emit volumeChanged(v);
        }
    });

    m_fullscreenBtn = new QPushButton(this);
    m_fullscreenBtn->setFixedSize(38, 38);
    m_fullscreenBtn->setCursor(Qt::PointingHandCursor);
    m_fullscreenBtn->setIcon(makeIcon(":/icons/resources/full_screen.svg"));
    m_fullscreenBtn->setIconSize(QSize(18, 18));
    m_fullscreenBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 19px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.1); }");

    m_speedBtn = new QPushButton(this);
    m_speedBtn->setFixedSize(44, 32);
    m_speedBtn->setCursor(Qt::PointingHandCursor);
    m_speedBtn->setToolTip("Playback speed");
    m_speedBtn->setFocusPolicy(Qt::NoFocus);
    m_speedBtn->setText("1.0x");
    m_speedBtn->setStyleSheet(
        "QPushButton { background: transparent; border: 1px solid rgba(255,255,255,0.2); "
        "border-radius: 16px; color: rgba(255,255,255,0.85); font-size: 12px; "
        "font-family: 'JetBrains Mono', 'Consolas', monospace; }"
        "QPushButton:hover { background: rgba(255,255,255,0.1); color: white; }");

    m_screenshotBtn = new QPushButton(this);
    m_screenshotBtn->setFixedSize(38, 38);
    m_screenshotBtn->setCursor(Qt::PointingHandCursor);
    m_screenshotBtn->setToolTip("Screenshot");
    m_screenshotBtn->setIcon(makeIcon(":/icons/resources/screenshot.svg"));
    m_screenshotBtn->setIconSize(QSize(18, 18));
    m_screenshotBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 19px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.1); }");

    m_settingsBtn = new QPushButton(this);
    m_settingsBtn->setFixedSize(38, 38);
    m_settingsBtn->setCursor(Qt::PointingHandCursor);
    m_settingsBtn->setToolTip("Settings");
    m_settingsBtn->setIcon(makeIcon(":/icons/resources/set.svg"));
    m_settingsBtn->setIconSize(QSize(18, 18));
    m_settingsBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 19px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.1); }");

    auto *leftLayout = new QHBoxLayout();
    leftLayout->setSpacing(6);
    leftLayout->addWidget(m_playBtn);
    leftLayout->addWidget(m_prevBtn);
    leftLayout->addWidget(m_nextBtn);
    leftLayout->addSpacing(8);
    leftLayout->addWidget(m_volumeBtn);

    auto *rightLayout = new QHBoxLayout();
    rightLayout->setSpacing(6);
    rightLayout->addWidget(m_speedBtn);
    rightLayout->addWidget(m_screenshotBtn);
    rightLayout->addWidget(m_fullscreenBtn);
    rightLayout->addWidget(m_settingsBtn);

    // Control-bar buttons are mouse-only; keep keyboard focus off them so the
    // Space/F/L/Esc shortcuts always reach the main window.
    for (QPushButton *btn : {m_playBtn, m_prevBtn, m_nextBtn, m_volumeBtn, m_fullscreenBtn, m_speedBtn, m_screenshotBtn, m_settingsBtn})
        btn->setFocusPolicy(Qt::NoFocus);
    m_volumeSlider->setFocusPolicy(Qt::NoFocus);
    m_seekSlider->setFocusPolicy(Qt::NoFocus);

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
    connect(m_speedBtn, &QPushButton::clicked, this, &ControlBar::onSpeedButtonClicked);
    connect(m_screenshotBtn, &QPushButton::clicked, this, &ControlBar::screenshotRequested);
    connect(m_settingsBtn, &QPushButton::clicked, this, &ControlBar::settingsRequested);

    // Speed popup menu: one checkable action per preset, current speed ticked.
    m_speedMenu = new QMenu(this);
    m_speedMenu->setStyleSheet(
        "QMenu { background-color: rgba(20,20,24,0.96); border: 1px solid rgba(255,255,255,0.12); "
        "border-radius: 8px; padding: 4px; }"
        "QMenu::item { color: rgba(255,255,255,0.85); padding: 6px 24px 6px 16px; "
        "border-radius: 4px; font-family: 'JetBrains Mono', 'Consolas', monospace; font-size: 13px; }"
        "QMenu::item:selected { background-color: rgba(124,58,237,0.35); color: white; }"
        "QMenu::indicator { width: 12px; }"
        "QMenu::indicator:checked { image: none; }");
    m_speedMenu->setWindowFlags(m_speedMenu->windowFlags() | Qt::FramelessWindowHint);
    m_speedMenu->setAttribute(Qt::WA_TranslucentBackground);
    for (int i = 0; i < kSpeedPresetCount; ++i) {
        QAction *act = m_speedMenu->addAction(speedLabel(kSpeedPresets[i]));
        act->setCheckable(true);
        act->setData(QVariant(kSpeedPresets[i]));
        if (i == m_speedIndex) act->setChecked(true);
    }
    connect(m_speedMenu, &QMenu::triggered, this, &ControlBar::onSpeedMenuTriggered);

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
    if (auto *vp = static_cast<VolumePopup *>(m_volumePopup))
        vp->setValue(volume);
    m_lastVolume = (volume > 0) ? volume : m_lastVolume;
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
    // Emit a single toggle signal; MainWindow decides pause/play based on the
    // actual player state, so the button never desyncs from the player when
    // async stateChanged signals reorder across a file switch.
    emit togglePlayRequested();
}

void ControlBar::onVolumeButtonClicked()
{
    // Show the vertical volume popup above the button.
    auto *vp = static_cast<VolumePopup *>(m_volumePopup);
    if (!vp) return;
    vp->setValue(m_volumeSlider->value());
    QPoint topRight = m_volumeBtn->mapToGlobal(QPoint(m_volumeBtn->width(), 0));
    int x = topRight.x() - vp->width() / 2 - m_volumeBtn->width() / 2;
    int y = topRight.y() - vp->height() - 6;
    vp->move(x, y);
    vp->show();
    vp->raise();
    vp->setFocus();
}

void ControlBar::updatePlayButton()
{
    m_playBtn->setIcon(QIcon(m_isPlaying ? ":/icons/resources/pause.svg"
                                         : ":/icons/resources/play.svg"));
    m_playBtn->setIconSize(QSize(22, 22));
}

void ControlBar::updateVolumeButton()
{
    int vol = m_volumeSlider->value();
    // Muted (volume 0) shows the mute icon; otherwise the volume icon.
    m_volumeBtn->setIcon(makeIcon(vol == 0 ? ":/icons/resources/mute.svg"
                                           : ":/icons/resources/volume.svg"));
    m_volumeBtn->setIconSize(QSize(18, 18));
}

void ControlBar::setSpeed(double speed)
{
    // Find the closest preset index so external speed changes (e.g. from the
    // player) keep the button label in sync.
    int best = 0;
    double bestDiff = 1e9;
    for (int i = 0; i < kSpeedPresetCount; ++i) {
        double diff = qAbs(kSpeedPresets[i] - speed);
        if (diff < bestDiff) { bestDiff = diff; best = i; }
    }
    m_speedIndex = best;
    // Keep the popup menu's check marks in sync too.
    const auto actions = m_speedMenu->actions();
    for (int i = 0; i < actions.size() && i < kSpeedPresetCount; ++i) {
        actions[i]->setChecked(i == m_speedIndex);
    }
    updateSpeedButton();
}

void ControlBar::updateSpeedButton()
{
    double s = kSpeedPresets[m_speedIndex];
    m_speedBtn->setText(speedLabel(s));
    // Highlight non-normal speeds so the user notices it is active.
    if (qFuzzyCompare(s, 1.0)) {
        m_speedBtn->setStyleSheet(
            "QPushButton { background: transparent; border: 1px solid rgba(255,255,255,0.2); "
            "border-radius: 16px; color: rgba(255,255,255,0.85); font-size: 12px; "
            "font-family: 'JetBrains Mono', 'Consolas', monospace; }"
            "QPushButton:hover { background: rgba(255,255,255,0.1); color: white; }");
    } else {
        m_speedBtn->setStyleSheet(
            "QPushButton { background: rgba(124,58,237,0.25); border: 1px solid rgba(124,58,237,0.6); "
            "border-radius: 16px; color: white; font-size: 12px; "
            "font-family: 'JetBrains Mono', 'Consolas', monospace; }"
            "QPushButton:hover { background: rgba(124,58,237,0.4); color: white; }");
    }
}

void ControlBar::onSpeedButtonClicked()
{
    // Popup the speed menu above the button.
    QPoint bottomLeft = m_speedBtn->mapToGlobal(QPoint(0, 0));
    QSize size = m_speedMenu->sizeHint();
    // Place the menu so its bottom aligns with the top of the button, with a
    // small gap. If it would go off the top of the screen, fall back to below.
    QPoint target(bottomLeft.x(), bottomLeft.y() - size.height() - 4);
    m_speedMenu->popup(target);
}

void ControlBar::onSpeedMenuTriggered(QAction *action)
{
    if (!action) return;
    bool ok = false;
    double s = action->data().toDouble(&ok);
    if (!ok) return;
    // Update the checked state of all actions (single-selection radio group).
    for (QAction *a : m_speedMenu->actions()) {
        a->setChecked(a == action);
    }
    // Sync m_speedIndex so updateSpeedButton() labels correctly.
    int best = 0;
    double bestDiff = 1e9;
    for (int i = 0; i < kSpeedPresetCount; ++i) {
        double diff = qAbs(kSpeedPresets[i] - s);
        if (diff < bestDiff) { bestDiff = diff; best = i; }
    }
    m_speedIndex = best;
    updateSpeedButton();
    emit speedChanged(s);
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
