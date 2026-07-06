#include "settings_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMouseEvent>
#include <QFileDialog>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QFrame>
#include <QIcon>
#include <QGraphicsDropShadowEffect>

namespace {
constexpr int kDialogWidth = 640;
constexpr int kDialogHeight = 420;
constexpr int kShadowMargin = 18;
}

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(kDialogWidth + kShadowMargin * 2, kDialogHeight + kShadowMargin * 2);

    m_panel = new QWidget(this);
    m_panel->setObjectName("settingsPanel");
    m_panel->setFixedSize(kDialogWidth, kDialogHeight);
    auto *shadowEffect = new QGraphicsDropShadowEffect(m_panel);
    shadowEffect->setBlurRadius(32);
    shadowEffect->setOffset(0, 2);
    shadowEffect->setColor(QColor(200, 200, 200, 150));
    m_panel->setGraphicsEffect(shadowEffect);

    // --- Title bar ---
    m_titleBar = new QWidget(m_panel);
    m_titleBar->setFixedHeight(40);
    m_titleBar->setObjectName("settingsTitleBar");
    m_titleBar->setStyleSheet(
        "QWidget#settingsTitleBar { background-color: #141418; border-top-left-radius: 8px; border-top-right-radius: 8px; }");

    m_titleLabel = new QLabel("Settings", m_titleBar);
    m_titleLabel->setStyleSheet("color: white; font-size: 14px; font-weight: 500;");

    m_closeBtn = new QPushButton(m_titleBar);
    m_closeBtn->setFixedSize(28, 28);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setToolTip("Close");
    m_closeBtn->setIcon(QIcon(QStringLiteral(":/icons/resources/close.svg")));
    m_closeBtn->setIconSize(QSize(14, 14));
    m_closeBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 6px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.1); }");

    auto *titleLayout = new QHBoxLayout(m_titleBar);
    titleLayout->setContentsMargins(16, 0, 8, 0);
    titleLayout->setSpacing(0);
    titleLayout->addWidget(m_titleLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(m_closeBtn);

    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::reject);

    // --- Left category list ---
    m_categoryList = new QListWidget(m_panel);
    m_categoryList->setFixedWidth(160);
    m_categoryList->setStyleSheet(
        "QListWidget { background-color: #0e0e12; border: none; outline: none; }"
        "QListWidget::item { color: rgba(255,255,255,0.7); padding: 14px 16px; font-size: 13px; }"
        "QListWidget::item:hover { background-color: rgba(255,255,255,0.05); color: white; }"
        "QListWidget::item:selected { background-color: rgba(124,58,237,0.20); color: white; "
        "border-left: 3px solid #7c3aed; }");
    m_categoryList->addItem("Screenshot");
    m_categoryList->setCurrentRow(0);

    // --- Right content stack ---
    m_stack = new QStackedWidget(m_panel);

    // Screenshot page
    auto *shotPage = new QWidget();
    shotPage->setStyleSheet("color: white; font-size: 13px;");
    auto *shotLayout = new QGridLayout(shotPage);
    shotLayout->setContentsMargins(28, 24, 28, 24);
    shotLayout->setVerticalSpacing(18);
    shotLayout->setHorizontalSpacing(12);

    QLabel *dirLabel = new QLabel("Screenshot folder:", shotPage);
    dirLabel->setStyleSheet("color: rgba(255,255,255,0.85);");
    m_screenshotDirEdit = new QLineEdit(shotPage);
    m_screenshotDirEdit->setReadOnly(true);
    m_screenshotDirEdit->setStyleSheet(
        "QLineEdit { background-color: #1a1a20; border: 1px solid rgba(255,255,255,0.12); "
        "border-radius: 6px; padding: 8px 10px; color: white; }");
    QPushButton *browseBtn = new QPushButton("Browse...", shotPage);
    browseBtn->setCursor(Qt::PointingHandCursor);
    browseBtn->setStyleSheet(
        "QPushButton { background-color: rgba(124,58,237,0.25); border: 1px solid rgba(124,58,237,0.6); "
        "border-radius: 6px; padding: 8px 16px; color: white; }"
        "QPushButton:hover { background-color: rgba(124,58,237,0.4); }");
    connect(browseBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseClicked);

    QLabel *fmtLabel = new QLabel("Image format:", shotPage);
    fmtLabel->setStyleSheet("color: rgba(255,255,255,0.85);");
    m_screenshotFormatCombo = new QComboBox(shotPage);
    m_screenshotFormatCombo->addItem("PNG", "png");
    m_screenshotFormatCombo->addItem("JPG", "jpg");
    m_screenshotFormatCombo->setStyleSheet(
        "QComboBox { background-color: #1a1a20; border: 1px solid rgba(255,255,255,0.12); "
        "border-radius: 6px; padding: 6px 10px; color: white; }"
        "QComboBox QAbstractItemView { background-color: #1a1a20; border: 1px solid rgba(255,255,255,0.12); "
        "selection-background-color: rgba(124,58,237,0.35); color: white; }");

    shotLayout->addWidget(dirLabel, 0, 0);
    shotLayout->addWidget(m_screenshotDirEdit, 0, 1);
    shotLayout->addWidget(browseBtn, 0, 2);
    shotLayout->addWidget(fmtLabel, 1, 0);
    shotLayout->addWidget(m_screenshotFormatCombo, 1, 1, 1, 2, Qt::AlignLeft);
    shotLayout->setColumnStretch(1, 1);
    shotLayout->setRowStretch(2, 1);

    m_stack->addWidget(shotPage);

    connect(m_categoryList, &QListWidget::currentRowChanged, this, &SettingsDialog::onCategoryChanged);

    // --- Bottom button row ---
    auto *bottomBar = new QWidget(m_panel);
    bottomBar->setFixedHeight(56);
    bottomBar->setStyleSheet("background-color: #0e0e12;");
    QPushButton *okBtn = new QPushButton("OK", bottomBar);
    QPushButton *cancelBtn = new QPushButton("Cancel", bottomBar);
    for (auto *b : {okBtn, cancelBtn}) {
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(34);
        b->setStyleSheet(
            "QPushButton { border-radius: 6px; padding: 0 20px; font-size: 13px; }");
    }
    okBtn->setStyleSheet(
        "QPushButton { background-color: #7c3aed; color: white; border: none; border-radius: 6px; padding: 0 22px; font-size: 13px; }"
        "QPushButton:hover { background-color: #6d28d9; }");
    cancelBtn->setStyleSheet(
        "QPushButton { background-color: transparent; color: rgba(255,255,255,0.7); border: 1px solid rgba(255,255,255,0.2); border-radius: 6px; padding: 0 18px; font-size: 13px; }"
        "QPushButton:hover { background-color: rgba(255,255,255,0.06); color: white; }");
    auto *bottomLayout = new QHBoxLayout(bottomBar);
    bottomLayout->setContentsMargins(20, 0, 20, 0);
    bottomLayout->addStretch();
    bottomLayout->setSpacing(10);
    bottomLayout->addWidget(cancelBtn);
    bottomLayout->addWidget(okBtn);

    connect(okBtn, &QPushButton::clicked, this, [this]() {
        saveSettings();
        accept();
    });
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    // --- Main layout ---
    auto *bodyLayout = new QHBoxLayout();
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);
    bodyLayout->addWidget(m_categoryList);
    auto *stackFrame = new QFrame();
    stackFrame->setStyleSheet("background-color: #0a0a0c;");
    auto *stackLayout = new QVBoxLayout(stackFrame);
    stackLayout->setContentsMargins(0, 0, 0, 0);
    stackLayout->addWidget(m_stack);
    bodyLayout->addWidget(stackFrame, 1);

    auto *mainLayout = new QVBoxLayout(m_panel);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(m_titleBar);
    mainLayout->addLayout(bodyLayout, 1);
    mainLayout->addWidget(bottomBar);

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(kShadowMargin, kShadowMargin, kShadowMargin, kShadowMargin);
    outerLayout->setSpacing(0);
    outerLayout->addWidget(m_panel);

    setStyleSheet("QWidget#settingsPanel { background-color: #0a0a0c; border-radius: 8px; }");

    loadSettings();
}

void SettingsDialog::loadSettings()
{
    QSettings s;
    QString dir = s.value("screenshot/dir").toString();
    if (dir.isEmpty()) {
        dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
              + QDir::separator() + "VideoPlayer" + QDir::separator() + "Screenshots";
    }
    m_screenshotDirEdit->setText(QDir::toNativeSeparators(dir));

    QString fmt = s.value("screenshot/format", "png").toString().toLower();
    int idx = (fmt == "jpg") ? 1 : 0;
    m_screenshotFormatCombo->setCurrentIndex(idx);
}

void SettingsDialog::saveSettings()
{
    QSettings s;
    s.setValue("screenshot/dir", m_screenshotDirEdit->text());
    int idx = m_screenshotFormatCombo->currentIndex();
    s.setValue("screenshot/format", m_screenshotFormatCombo->itemData(idx).toString());
}

QString SettingsDialog::screenshotDir()
{
    QSettings s;
    QString dir = s.value("screenshot/dir").toString();
    if (dir.isEmpty()) {
        dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
              + QDir::separator() + "VideoPlayer" + QDir::separator() + "Screenshots";
    }
    return dir;
}

QString SettingsDialog::screenshotFormat()
{
    QSettings s;
    QString fmt = s.value("screenshot/format", "png").toString().toLower();
    return (fmt == "jpg") ? "jpg" : "png";
}

void SettingsDialog::onBrowseClicked()
{
    QString cur = m_screenshotDirEdit->text();
    QString dir = QFileDialog::getExistingDirectory(this, "Select screenshot folder", cur);
    if (!dir.isEmpty()) {
        m_screenshotDirEdit->setText(QDir::toNativeSeparators(dir));
    }
}

void SettingsDialog::onCategoryChanged(int row)
{
    if (row >= 0 && row < m_stack->count()) {
        m_stack->setCurrentIndex(row);
    }
}

void SettingsDialog::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // Only start dragging when pressing on the title bar area.
        const QRect titleRect(m_titleBar->mapTo(this, QPoint(0, 0)), m_titleBar->size());
        if (titleRect.contains(event->pos())) {
            m_dragPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
            event->accept();
            return;
        }
    }
    QDialog::mousePressEvent(event);
}

void SettingsDialog::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton && !m_dragPos.isNull()) {
        move(event->globalPosition().toPoint() - m_dragPos);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}
