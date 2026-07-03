#include "slider.h"
#include <QStyle>
#include <QStyleOptionSlider>

SeekSlider::SeekSlider(Qt::Orientation orientation, QWidget *parent)
    : QSlider(orientation, parent)
{
    setRange(0, 1000);
    setSingleStep(1);
    setMouseTracking(true);
}

int SeekSlider::valueFromPos(int pos) const
{
    QStyleOptionSlider opt;
    initStyleOption(&opt);
    QRect grooveRect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
    QRect handleRect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);

    int sliderMin, sliderMax;
    if (orientation() == Qt::Horizontal) {
        sliderMin = grooveRect.left() + handleRect.width() / 2;
        sliderMax = grooveRect.right() - handleRect.width() / 2;
    } else {
        sliderMin = grooveRect.top() + handleRect.height() / 2;
        sliderMax = grooveRect.bottom() - handleRect.height() / 2;
    }

    int val;
    if (orientation() == Qt::Horizontal) {
        val = QStyle::sliderValueFromPosition(minimum(), maximum(), pos - sliderMin,
                                               sliderMax - sliderMin, opt.upsideDown);
    } else {
        val = QStyle::sliderValueFromPosition(minimum(), maximum(), pos - sliderMin,
                                               sliderMax - sliderMin, opt.upsideDown);
    }
    return val;
}

void SeekSlider::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        int val = valueFromPos(event->pos().x());
        setValue(val);
        emit sliderMoved(val);
        event->accept();
    } else {
        QSlider::mousePressEvent(event);
    }
}

void SeekSlider::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        int val = valueFromPos(event->pos().x());
        setValue(val);
        emit sliderMoved(val);
    }
    QSlider::mouseMoveEvent(event);
}

void SeekSlider::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        int val = valueFromPos(event->pos().x());
        setValue(val);
        emit seekRequested(val);
        event->accept();
    } else {
        QSlider::mouseReleaseEvent(event);
    }
}

// VolumeSlider
VolumeSlider::VolumeSlider(Qt::Orientation orientation, QWidget *parent)
    : QSlider(orientation, parent)
{
    setRange(0, 100);
    setSingleStep(1);
}

void VolumeSlider::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QStyleOptionSlider opt;
        initStyleOption(&opt);
        QRect grooveRect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
        QRect handleRect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);

        int sliderMin = grooveRect.left() + handleRect.width() / 2;
        int sliderMax = grooveRect.right() - handleRect.width() / 2;

        int val = QStyle::sliderValueFromPosition(minimum(), maximum(),
                                                   event->pos().x() - sliderMin,
                                                   sliderMax - sliderMin, opt.upsideDown);
        setValue(val);
        emit sliderMoved(val);
        event->accept();
    } else {
        QSlider::mousePressEvent(event);
    }
}
