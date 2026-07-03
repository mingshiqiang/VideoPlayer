#ifndef VIDEO_CANVAS_H
#define VIDEO_CANVAS_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QTimer>
#include <QImage>

class VideoFrameQueue;

class VideoCanvas : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
    Q_OBJECT
public:
    explicit VideoCanvas(QWidget *parent = nullptr);
    ~VideoCanvas();

    void setVideoFrame(const QImage &frame);
    void clearFrame();
    void setAutoRefresh(bool enabled);
    void setVideoFrameQueue(VideoFrameQueue *queue);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private slots:
    void onRefresh();

private:
    QOpenGLShaderProgram *m_program = nullptr;
    QOpenGLTexture *m_texture = nullptr;
    QTimer *m_refreshTimer = nullptr;

    QImage m_currentFrame;
    bool m_hasFrame = false;
    bool m_frameChanged = false;

    VideoFrameQueue *m_queue = nullptr;

    GLuint m_vao = 0;
    GLuint m_vbo = 0;
};

#endif // VIDEO_CANVAS_H
