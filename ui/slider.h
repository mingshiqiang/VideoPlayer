#ifndef SLIDER_H
#define SLIDER_H

#include <QSlider>
#include <QMouseEvent>
#include <QStyle>
#include <QStyleOptionSlider>

class SeekSlider : public QSlider {
    Q_OBJECT
public:
    explicit SeekSlider(Qt::Orientation orientation, QWidget *parent = nullptr);

signals:
    void seekRequested(qint64 positionMs);
    void sliderHovered(qint64 positionMs);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    int valueFromPos(int pos) const;
    bool m_dragging = false;
};

class VolumeSlider : public QSlider {
    Q_OBJECT
public:
    explicit VolumeSlider(Qt::Orientation orientation, QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
};

#endif // SLIDER_H
