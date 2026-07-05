#include "titlebar.h"
#include <QHBoxLayout>
#include <QEvent>
#include <QPainter>
#include <QLinearGradient>
#include <QStyle>
#include <QIcon>
#include <QPixmap>

namespace {
// Load an SVG icon from the compiled resource, scaled to 16x16 for the
// 28x28 title-bar buttons.
QIcon makeIcon(const char *resPath)
{
    QIcon icon(QString::fromUtf8(resPath));
    return icon;
}
}  // namespace

TitleBar::TitleBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(40);
    setAttribute(Qt::WA_TranslucentBackground);

    m_logoLabel = new QLabel(this);
    m_logoLabel->setFixedSize(24, 24);
    m_logoLabel->setAlignment(Qt::AlignCenter);
    {
        // Render the SVG logo to a pixmap at 2x for crisp rendering on HiDPI.
        const int logoPx = 24;
        QPixmap pm = QIcon(QStringLiteral(":/icons/resources/logo.svg"))
                         .pixmap(QSize(logoPx, logoPx));
        m_logoLabel->setPixmap(pm);
    }

    m_titleLabel = new QLabel("VideoPlayer", this);

    m_openBtn = new QPushButton(this);
    m_openBtn->setFixedSize(28, 28);
    m_openBtn->setCursor(Qt::PointingHandCursor);
    m_openBtn->setToolTip("Open file (Ctrl+O)");
    m_openBtn->setIcon(makeIcon(":/icons/resources/load_video.svg"));
    m_openBtn->setIconSize(QSize(16, 16));

    m_playlistBtn = new QPushButton(this);
    m_playlistBtn->setFixedSize(28, 28);
    m_playlistBtn->setCursor(Qt::PointingHandCursor);
    m_playlistBtn->setToolTip("Toggle playlist (L)");
    m_playlistBtn->setCheckable(true);
    m_playlistBtn->setChecked(true);
    m_playlistBtn->setIcon(makeIcon(":/icons/resources/expand_list.svg"));
    m_playlistBtn->setIconSize(QSize(16, 16));

    m_minBtn = new QPushButton(this);
    m_minBtn->setFixedSize(28, 28);
    m_minBtn->setCursor(Qt::PointingHandCursor);
    m_minBtn->setIcon(makeIcon(":/icons/resources/minimize.svg"));
    m_minBtn->setIconSize(QSize(16, 16));

    m_maxBtn = new QPushButton(this);
    m_maxBtn->setFixedSize(28, 28);
    m_maxBtn->setCursor(Qt::PointingHandCursor);
    m_maxBtn->setIcon(makeIcon(":/icons/resources/max.svg"));
    m_maxBtn->setIconSize(QSize(16, 16));

    m_closeBtn = new QPushButton(this);
    m_closeBtn->setFixedSize(28, 28);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setIcon(makeIcon(":/icons/resources/close.svg"));
    m_closeBtn->setIconSize(QSize(16, 16));

    m_titleLabel->setStyleSheet("color: white; font-size: 13px; font-weight: 500;");

    m_openBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 6px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.1); }");

    m_playlistBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 6px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.1); }");

    m_minBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 6px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.1); }");

    m_maxBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 6px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.1); }");

    m_closeBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 6px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.1); }");

    // Title-bar buttons are mouse-only; keep keyboard focus off them so that
    // global shortcuts (Space/F/L/...) work and no button shows a focus rect.
    for (QPushButton *btn : {m_openBtn, m_playlistBtn, m_minBtn, m_maxBtn, m_closeBtn})
        btn->setFocusPolicy(Qt::NoFocus);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 0, 10, 0);
    layout->setSpacing(8);
    layout->addWidget(m_logoLabel);
    layout->addWidget(m_titleLabel);
    layout->addStretch();
    layout->addWidget(m_openBtn);
    layout->addWidget(m_playlistBtn);
    layout->addWidget(m_minBtn);
    layout->addWidget(m_maxBtn);
    layout->addWidget(m_closeBtn);

    connect(m_openBtn, &QPushButton::clicked, this, &TitleBar::openFileClicked);
    connect(m_playlistBtn, &QPushButton::clicked, this, &TitleBar::togglePlaylistClicked);
    connect(m_minBtn, &QPushButton::clicked, this, &TitleBar::minimizeClicked);
    connect(m_maxBtn, &QPushButton::clicked, this, &TitleBar::maximizeClicked);
    connect(m_closeBtn, &QPushButton::clicked, this, &TitleBar::closeClicked);
}

void TitleBar::setFileName(const QString &name)
{
    m_titleLabel->setText(name.isEmpty() ? "VideoPlayer" : name);
}

void TitleBar::setActive(bool active)
{
    QString color = active ? "white" : "rgba(255,255,255,0.5)";
    m_titleLabel->setStyleSheet(QString("color: %1; font-size: 13px; font-weight: 500;").arg(color));
}

void TitleBar::setMaximized(bool maximized)
{
    m_isMaximized = maximized;
    m_maxBtn->setIcon(makeIcon(maximized ? ":/icons/resources/normal.svg"
                                         : ":/icons/resources/max.svg"));
    m_maxBtn->setIconSize(QSize(16, 16));
}

void TitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragStartPos = event->globalPosition().toPoint();
        event->accept();
    }
}

void TitleBar::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        QPoint delta = event->globalPosition().toPoint() - m_dragStartPos;
        QWidget *win = window();
        if (win && !win->isMaximized()) {
            win->move(win->pos() + delta);
        }
        m_dragStartPos = event->globalPosition().toPoint();
        event->accept();
    }
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit maximizeClicked();
        event->accept();
    }
}

void TitleBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    QLinearGradient gradient(0, 0, 0, height());
    gradient.setColorAt(0, QColor(0, 0, 0, 80));
    gradient.setColorAt(1, QColor(0, 0, 0, 0));
    p.fillRect(rect(), gradient);
}
