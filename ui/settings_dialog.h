#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QStackedWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    // Persisted settings read/written on construction / accept.
    static QString screenshotDir();
    static QString screenshotFormat();   // "png" or "jpg"

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onBrowseClicked();
    void onCategoryChanged(int row);

private:
    void loadSettings();
    void saveSettings();

    QWidget *m_titleBar = nullptr;
    QLabel *m_titleLabel = nullptr;
    QPushButton *m_closeBtn = nullptr;
    QListWidget *m_categoryList = nullptr;
    QStackedWidget *m_stack = nullptr;

    // Screenshot page widgets
    QLineEdit *m_screenshotDirEdit = nullptr;
    QComboBox *m_screenshotFormatCombo = nullptr;

    QPoint m_dragPos;
};

#endif // SETTINGS_DIALOG_H
