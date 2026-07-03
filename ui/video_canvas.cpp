#include "video_canvas.h"
#include "../core/video_frame_queue.h"
#include <QPainter>
#include <QResizeEvent>
#include <QImage>

static const char *kVertexShader = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
out vec2 TexCoord;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    TexCoord = aTexCoord;
}
)";

static const char *kFragmentShader = R"(
#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D tex;
void main() {
    FragColor = texture(tex, TexCoord);
}
)";

static const float kQuadVertices[] = {
    -1.0f, -1.0f,  0.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 1.0f,
    -1.0f,  1.0f,  0.0f, 0.0f,

    -1.0f,  1.0f,  0.0f, 0.0f,
     1.0f, -1.0f,  1.0f, 1.0f,
     1.0f,  1.0f,  1.0f, 0.0f,
};

VideoCanvas::VideoCanvas(QWidget *parent)
    : QOpenGLWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(320, 180);
    setAutoFillBackground(false);

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(16);
    connect(m_refreshTimer, &QTimer::timeout, this, &VideoCanvas::onRefresh);
    m_refreshTimer->start();
}

VideoCanvas::~VideoCanvas()
{
    makeCurrent();
    if (m_texture) { delete m_texture; m_texture = nullptr; }
    if (m_program) { delete m_program; m_program = nullptr; }
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    doneCurrent();
}

void VideoCanvas::initializeGL()
{
    initializeOpenGLFunctions();

    glClearColor(0.04f, 0.04f, 0.05f, 1.0f);

    m_program = new QOpenGLShaderProgram(this);
    m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader);
    m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, kFragmentShader);
    m_program->link();

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void VideoCanvas::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void VideoCanvas::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT);

    if (!m_hasFrame || !m_texture) {
        return;
    }

    if (m_frameChanged) {
        // Convert RGB24 QImage to RGBA for OpenGL texture
        QImage rgba = m_currentFrame.convertToFormat(QImage::Format_RGBA8888);
        m_texture->setData(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, rgba.constBits());
        m_frameChanged = false;
    }

    m_program->bind();
    m_texture->bind(0);
    m_program->setUniformValue("tex", 0);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    m_program->release();
}

void VideoCanvas::setVideoFrame(const QImage &frame)
{
    m_currentFrame = frame;
    m_hasFrame = true;
    m_frameChanged = true;

    // Create texture if not exists or size changed
    if (!m_texture || m_texture->width() != frame.width() || m_texture->height() != frame.height()) {
        makeCurrent();
        if (m_texture) delete m_texture;
        m_texture = new QOpenGLTexture(QOpenGLTexture::Target2D);
        m_texture->setSize(frame.width(), frame.height());
        m_texture->setFormat(QOpenGLTexture::RGBA8_UNorm);
        m_texture->setMinificationFilter(QOpenGLTexture::Linear);
        m_texture->setMagnificationFilter(QOpenGLTexture::Linear);
        m_texture->allocateStorage();
        doneCurrent();
    }
}

void VideoCanvas::setVideoFrameQueue(VideoFrameQueue *queue)
{
    m_queue = queue;
}

void VideoCanvas::clearFrame()
{
    m_hasFrame = false;
    m_frameChanged = false;
    update();
}

void VideoCanvas::setAutoRefresh(bool enabled)
{
    if (enabled)
        m_refreshTimer->start();
    else
        m_refreshTimer->stop();
}

void VideoCanvas::onRefresh()
{
    if (m_queue) {
        VideoFrame vf;
        if (m_queue->pop(vf, 0)) {
            setVideoFrame(vf.image);
        }
    }
    if (m_frameChanged) {
        update();
    }
}
