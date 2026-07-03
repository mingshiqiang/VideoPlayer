#include "playlist_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QPainter>
#include <QMimeData>

PlaylistWidget::PlaylistWidget(QWidget *parent)
    : QWidget(parent)
{
    setFixedWidth(300);
    setAttribute(Qt::WA_TranslucentBackground, false);

    m_headerLabel = new QLabel("Playlist", this);
    m_headerLabel->setStyleSheet(
        "color: white; font-size: 14px; font-weight: 500;");

    m_countLabel = new QLabel("0 items", this);
    m_countLabel->setStyleSheet(
        "color: rgba(255,255,255,0.45); font-size: 12px;");

    m_closeBtn = new QPushButton(this);
    m_closeBtn->setFixedSize(28, 28);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setToolTip("Collapse playlist");
    m_closeBtn->setText(">");
    m_closeBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; "
        "color: rgba(255,255,255,0.7); font-size: 14px; border-radius: 6px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.1); color: white; }");

    auto *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(16, 0, 12, 0);
    headerLayout->addWidget(m_headerLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(m_countLabel);
    headerLayout->addWidget(m_closeBtn);

    m_listWidget = new QListWidget(this);
    m_listWidget->setStyleSheet(
        "QListWidget { background-color: transparent; border: none; outline: none; }"
        "QListWidget::item { color: white; border-bottom: 1px solid rgba(255,255,255,0.04); }"
        "QListWidget::item:hover { background-color: rgba(255,255,255,0.08); }"
        "QListWidget::item:selected { background-color: rgba(255,255,255,0.08); }");
    m_listWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listWidget->setSpacing(0);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Header container
    auto *headerWidget = new QWidget(this);
    headerWidget->setFixedHeight(52);
    headerWidget->setLayout(headerLayout);
    mainLayout->addWidget(headerWidget);
    mainLayout->addWidget(m_listWidget, 1);

    connect(m_closeBtn, &QPushButton::clicked, this, &PlaylistWidget::onCloseClicked);
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, &PlaylistWidget::onItemDoubleClicked);
}

void PlaylistWidget::addFile(const QString &filePath)
{
    PlaylistItem item;
    item.filePath = filePath;
    item.fileName = QFileInfo(filePath).fileName();
    m_items.append(item);

    auto *listItem = new QListWidgetItem(m_listWidget);
    listItem->setText(item.fileName);
    listItem->setToolTip(item.filePath);
    listItem->setSizeHint(QSize(0, 56));

    updateCountLabel();
}

void PlaylistWidget::addFiles(const QStringList &filePaths)
{
    for (const QString &path : filePaths) {
        addFile(path);
    }
}

void PlaylistWidget::removeCurrent()
{
    if (m_currentIndex < 0 || m_currentIndex >= m_items.size()) return;

    m_items.removeAt(m_currentIndex);
    delete m_listWidget->takeItem(m_currentIndex);

    if (m_currentIndex >= m_items.size()) {
        m_currentIndex = m_items.size() - 1;
    }

    updateItemHighlight();
    updateCountLabel();

    if (m_items.isEmpty()) {
        m_currentIndex = -1;
        emit playlistEmptied();
    }
}

void PlaylistWidget::clear()
{
    m_items.clear();
    m_listWidget->clear();
    m_currentIndex = -1;
    updateCountLabel();
}

int PlaylistWidget::count() const
{
    return m_items.size();
}

int PlaylistWidget::currentIndex() const
{
    return m_currentIndex;
}

QString PlaylistWidget::currentFilePath() const
{
    if (m_currentIndex >= 0 && m_currentIndex < m_items.size()) {
        return m_items[m_currentIndex].filePath;
    }
    return QString();
}

QString PlaylistWidget::filePathAt(int index) const
{
    if (index >= 0 && index < m_items.size()) {
        return m_items[index].filePath;
    }
    return QString();
}

QStringList PlaylistWidget::allFilePaths() const
{
    QStringList paths;
    for (const auto &item : m_items) {
        paths << item.filePath;
    }
    return paths;
}

void PlaylistWidget::setCurrentIndex(int index)
{
    if (index < 0 || index >= m_items.size()) return;
    m_currentIndex = index;
    updateItemHighlight();
    m_listWidget->scrollToItem(m_listWidget->item(index), QAbstractItemView::EnsureVisible);
}

void PlaylistWidget::setDurationForCurrent(qint64 durationMs)
{
    if (m_currentIndex >= 0 && m_currentIndex < m_items.size()) {
        m_items[m_currentIndex].durationMs = durationMs;
        auto *item = m_listWidget->item(m_currentIndex);
        if (item) {
            QString dur = formatDuration(durationMs);
            item->setText(QString("%1    [%2]").arg(m_items[m_currentIndex].fileName, dur));
        }
    }
}

void PlaylistWidget::setPlaylistVisible(bool visible)
{
    m_visible = visible;
    if (visible) {
        show();
    } else {
        hide();
    }
}

void PlaylistWidget::onItemDoubleClicked(QListWidgetItem *item)
{
    int row = m_listWidget->row(item);
    if (row >= 0 && row < m_items.size()) {
        m_currentIndex = row;
        updateItemHighlight();
        emit itemDoubleClicked(row);
    }
}

void PlaylistWidget::onCloseClicked()
{
    emit closeRequested();
}

void PlaylistWidget::updateCountLabel()
{
    int n = m_items.size();
    m_countLabel->setText(QString("%1 %2").arg(n).arg(n == 1 ? "item" : "items"));
}

void PlaylistWidget::updateItemHighlight()
{
    for (int i = 0; i < m_listWidget->count(); ++i) {
        auto *item = m_listWidget->item(i);
        if (i == m_currentIndex) {
            item->setBackground(QColor(124, 58, 237, 30));
            QFont f = item->font();
            f.setBold(true);
            item->setFont(f);
        } else {
            item->setBackground(Qt::transparent);
            QFont f = item->font();
            f.setBold(false);
            item->setFont(f);
        }
    }
}

QString PlaylistWidget::formatDuration(qint64 ms) const
{
    qint64 totalSec = ms / 1000;
    int h = totalSec / 3600;
    int m = (totalSec % 3600) / 60;
    int s = totalSec % 60;
    if (h > 0) return QString::asprintf("%d:%02d:%02d", h, m, s);
    return QString::asprintf("%02d:%02d", m, s);
}

void PlaylistWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.fillRect(rect(), QColor(20, 20, 24, 225));
    p.setPen(QColor(255, 255, 255, 15));
    p.drawLine(0, 0, 0, height());
}
