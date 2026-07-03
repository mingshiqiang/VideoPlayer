#ifndef PLAYLIST_WIDGET_H
#define PLAYLIST_WIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QPropertyAnimation>
#include <QStringList>
#include <QPaintEvent>

struct PlaylistItem {
    QString filePath;
    QString fileName;
    qint64 durationMs = 0;
};

class PlaylistWidget : public QWidget {
    Q_OBJECT
public:
    explicit PlaylistWidget(QWidget *parent = nullptr);

    void addFile(const QString &filePath);
    void addFiles(const QStringList &filePaths);
    void removeCurrent();
    void clear();
    int count() const;
    int currentIndex() const;
    QString currentFilePath() const;
    QString filePathAt(int index) const;
    QStringList allFilePaths() const;

    void setCurrentIndex(int index);
    void setPlaylistVisible(bool visible);
    bool isPlaylistVisible() const { return m_visible; }

    void setDurationForCurrent(qint64 durationMs);

signals:
    void itemDoubleClicked(int index);
    void closeRequested();
    void playlistEmptied();

private slots:
    void onItemDoubleClicked(QListWidgetItem *item);
    void onCloseClicked();

private:
    QLabel *m_headerLabel;
    QLabel *m_countLabel;
    QPushButton *m_closeBtn;
    QListWidget *m_listWidget;

    QList<PlaylistItem> m_items;
    int m_currentIndex = -1;
    bool m_visible = true;

    QString formatDuration(qint64 ms) const;
    void updateCountLabel();
    void updateItemHighlight();

protected:
    void paintEvent(QPaintEvent *event) override;
};

#endif // PLAYLIST_WIDGET_H
