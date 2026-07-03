#ifndef TITLEBAR_H
#define TITLEBAR_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QMouseEvent>
#include <QPaintEvent>

class TitleBar : public QWidget {
    Q_OBJECT
public:
    explicit TitleBar(QWidget *parent = nullptr);

    void setFileName(const QString &name);
    void setActive(bool active);

signals:
    void minimizeClicked();
    void maximizeClicked();
    void closeClicked();
    void openFileClicked();
    void togglePlaylistClicked();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QLabel *m_logoLabel;
    QLabel *m_titleLabel;
    QPushButton *m_openBtn;
    QPushButton *m_playlistBtn;
    QPushButton *m_minBtn;
    QPushButton *m_maxBtn;
    QPushButton *m_closeBtn;

    QPoint m_dragStartPos;
    bool m_isMaximized = false;
};

#endif // TITLEBAR_H
