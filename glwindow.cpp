#include "glwindow.h"
#include <QImage>
#include <QOpenGLTexture>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLContext>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLExtraFunctions>
#include <QPropertyAnimation>
#include <QPauseAnimation>
#include <QSequentialAnimationGroup>
#include <QTimer>


GLWindow::GLWindow()
    : m_texture(0),
      m_program(0),
      m_vbo(0),
      m_vao(0),
      m_target(0, 0, -1),
      m_uniformsDirty(true),
      m_r(0),
      m_r2(0)
{
    m_world.setToIdentity();
    m_world.translate(0, 0, -1);
    m_world.rotate(180, 1, 0, 0);
    m_mousePressed = false;
    m_vertices.reserve(448.0 * 784.0);
}

GLWindow::~GLWindow()
{
    makeCurrent();
    delete m_texture;
    delete m_program;
    delete m_vbo;
    delete m_vao;
}

static const char *vertexShaderSource =
    "layout(location = 0) in vec3 vertex;\n"
    //"layout(location = 1) in vec3 normal;\n"

    "uniform mat4 projMatrix;\n"
    "uniform mat4 camMatrix;\n"
    "uniform mat4 worldMatrix;\n"
    "uniform vec2 translation;\n"

    "out vec2 texCoord;\n"

    "void main() {\n"
        "vec3 translatedVertex = vertex;\n"
        "translatedVertex.xy += translation;  // Apply the translation to x and y coordinates\n"
        "texCoord = vec2(vertex.x / 784.0, vertex.y / 448.0);\n"
        "gl_Position = projMatrix * camMatrix * worldMatrix * vec4(translatedVertex, 1.0);\n"
    "}\n";

static const char *fragmentShaderSource =
    "in vec2 texCoord;\n"
    "out vec4 fragColor;\n"

    "uniform sampler2D textureSampler;\n"

    "void main() {\n"
        "vec3 color = texture(textureSampler, texCoord).rgb;\n"
        "fragColor = vec4(color, 1.0);\n"
    "}\n";


QByteArray versionedShaderCode(const char *src)
{
    QByteArray versionedSrc;

    if (QOpenGLContext::currentContext()->isOpenGLES())
        versionedSrc.append(QByteArrayLiteral("#version 300 es\n"));
    else
        versionedSrc.append(QByteArrayLiteral("#version 330\n"));
        

    versionedSrc.append(src);
    return versionedSrc;
}

void GLWindow::initializeGL() {
    std::cout << __func__ << std::endl;
    makeCurrent();
    
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    if (!f) {
        qDebug() << "Failed to initialize OpenGL functions!";
        return;
    }

    qDebug() << "Compiling and linking shaders...";
    m_program = new QOpenGLShaderProgram();
    m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, versionedShaderCode(vertexShaderSource));
    m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, versionedShaderCode(fragmentShaderSource));
    if (!m_program->link()) {
        qDebug() << "Shader program link error:" << m_program->log();
        return;
    }

    qDebug() << "Shaders compiled and linked.";
    m_program->bind();
    m_projMatrixLoc = m_program->uniformLocation("projMatrix");
    m_camMatrixLoc = m_program->uniformLocation("camMatrix");
    m_worldMatrixLoc = m_program->uniformLocation("worldMatrix");
    m_translation = m_program->uniformLocation("translation");

    qDebug() << "Creating VAO...";
    m_vao = new QOpenGLVertexArrayObject();
    if (!m_vao->create()) {
        qDebug() << "Failed to create VAO!";
        return;
    }
    m_vao->bind();
    qDebug() << "VAO created and bound.";

    qDebug() << "Creating VBO...";
    m_vbo = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    if (!m_vbo->create()) {
        qDebug() << "Failed to create VBO!";
        return;
    }
    m_vbo->bind();
    
    qDebug() << "Loading initial depth map...";
    cv::Mat initialDepthMap = cv::Mat::zeros(448, 784, CV_32FC1);
    //cv::imread("../NFOV/boston_narrow_base/Depth_RAW.exr", cv::IMREAD_UNCHANGED);
    if (initialDepthMap.empty()) {
        qDebug() << "Failed to load initial depth map!";
        return;
    }

    float centerX = (initialDepthMap.cols - 1) / 2.0f;
    float centerY = (initialDepthMap.rows - 1) / 2.0f;

    for (int y = 0; y < initialDepthMap.rows; ++y) {
        for (int x = 0; x < initialDepthMap.cols; ++x) {
            float z = initialDepthMap.at<float>(y, x);
            m_vertices.push_back(static_cast<float>(x));
            m_vertices.push_back(static_cast<float>(y));
            m_vertices.push_back(z);
        }
    }

    m_eye = QVector3D(0, 0, 500.0f);  // Move the camera farther away along the z-axis
    QVector2D centeredTranslation(-centerX, -centerY);  // Center the image
    m_program->setUniformValue(m_translation, centeredTranslation); 

    qDebug() << "Allocating VBO...";
    m_vbo->allocate(m_vertices.data(), m_vertices.size() * sizeof(GLfloat));

    qDebug() << "Setting vertex attributes...";
    f->glEnableVertexAttribArray(0);
    f->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), 0);

    if (!m_texture) {
        qDebug() << "Creating a new texture...";
        m_texture = new QOpenGLTexture(QOpenGLTexture::Target2D);
        m_texture->create();  // Explicitly create the texture
        m_texture->setSize(784, 448);  // Set the size to match your image
        m_texture->setFormat(QOpenGLTexture::RGB8_UNorm);  // Set format
        m_texture->allocateStorage();  // Allocate storage for the texture
    }

    if (m_texture->isCreated()) {
        qDebug() << "Texture successfully created.";
    } else {
        qDebug() << "Failed to create texture!";
    }

    
    m_vbo->release();
    m_vao->release();

    qDebug() << "Setting up depth test and face culling...";
    f->glEnable(GL_DEPTH_TEST);
    f->glEnable(GL_CULL_FACE);

    m_program->release();
    doneCurrent();
    qDebug() << "Initialization complete.";
}

void GLWindow::updateFrame(const cv::Mat& newDepthMap, const cv::Mat& newRGBImage) {
    qDebug() << "Entering updateFrame...";
    if (newDepthMap.empty() || newDepthMap.type() != CV_32FC1) {
        qDebug() << "Invalid depth map!";
        return;
    }

    if (newRGBImage.empty() || newRGBImage.type() != CV_8UC3) {
        qDebug() << "Invalid RGB image!";
        return;
    }

    makeCurrent(); 
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    if (!f) {
        qDebug() << "Failed to get OpenGL functions!";
        return;
    }

    m_vertices.clear();

    qDebug() << "Processing depth map...";
    for (int y = 0; y < newDepthMap.rows; ++y) {
        for (int x = 0; x < newDepthMap.cols; ++x) {
            float z = newDepthMap.at<float>(y, x);
            m_vertices.push_back(static_cast<float>(x));
            m_vertices.push_back(static_cast<float>(y));
            m_vertices.push_back(z);
        }
    }

    qDebug() << "Updating VBO...";
    m_vbo->bind();
    m_vbo->write(0, m_vertices.data(), m_vertices.size() * sizeof(GLfloat));
    m_vbo->release();

    qDebug() << "Updating texture...";
    if (newRGBImage.type() == CV_8UC3) {
        qDebug() << "bind()";
        m_texture->bind();
        cv::Mat rgbImage;
        cv::cvtColor(newRGBImage, rgbImage, cv::COLOR_BGR2RGB);
        qDebug() << "setData()";
        m_texture->setData(QOpenGLTexture::RGB, QOpenGLTexture::UInt8, rgbImage.data);
        qDebug() << "release";
        m_texture->release();
    }

    update();  // Trigger a redraw
    doneCurrent();
    qDebug() << "Exiting updateFrame.";
}



void GLWindow::resizeGL(int w, int h)
{
    m_proj.setToIdentity();
    //m_proj.perspective(45.0f, GLfloat(w) / h, 0.01f, 1000.0f);
    m_proj.perspective(45.0f, GLfloat(w) / h, 0.01f, 5000.0f);
    m_uniformsDirty = true;
}

void GLWindow::paintGL()
{
    QOpenGLExtraFunctions *f = QOpenGLContext::currentContext()->extraFunctions();

    f->glClearColor(0, 0, 0, 1);
    f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_program->bind();
    m_texture->bind(0);  // Bind the texture to texture unit 0
    m_program->setUniformValue("textureSampler", 0);  // Set the uniform sampler to use texture unit 0

    if (m_uniformsDirty) {
        m_uniformsDirty = false;
        QMatrix4x4 camera;
        camera.lookAt(m_eye, m_eye + m_target, QVector3D(0, 1, 0));
        m_program->setUniformValue(m_projMatrixLoc, m_proj);
        m_program->setUniformValue(m_camMatrixLoc, camera);
        QMatrix4x4 wm = m_world;
        wm.rotate(m_yaw, 0, 1, 0);  // Rotate around y-axis based on yaw
        wm.rotate(m_pitch, 1, 0, 0);  // Rotate around x-axis based on pitch
        m_program->setUniformValue(m_worldMatrixLoc, wm);
    }

    m_vao->bind();  // Ensure the VAO is bound
    f->glDrawArrays(GL_POINTS, 0, m_vertices.size() / 3);
}

void GLWindow::mousePressEvent(QMouseEvent *event)
{
    m_lastMousePosition = event->pos();
    m_mousePressed = true;
}

void GLWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_mousePressed)
    {
        int dx = event->x() - m_lastMousePosition.x();
        int dy = event->y() - m_lastMousePosition.y();

        m_yaw += dx * 0.5f;  // Adjust the sensitivity as needed
        m_pitch += dy * 0.5f;

        m_lastMousePosition = event->pos();

        m_uniformsDirty = true;
        update();  // Trigger a redraw
    }
}

void GLWindow::mouseReleaseEvent(QMouseEvent *event)
{
    m_mousePressed = false;
}

void GLWindow::keyPressEvent(QKeyEvent *event)
{
    float step = 5.0;//0.5f;  // Adjust this step value as needed

    switch (event->key()) {
    case Qt::Key_W:
        m_eye.setY(m_eye.y() - step);   // Move up
        break;
    case Qt::Key_S:
        m_eye.setY(m_eye.y() + step);   // Move down
        break;
    case Qt::Key_A:
        m_eye.setX(m_eye.x() + step);  // Pan left
        break;
    case Qt::Key_D:
        m_eye.setX(m_eye.x() - step);  // Pan right
        break;
    default:
        QOpenGLWindow::keyPressEvent(event);  // Call the base class implementation for other keys
    }

    m_uniformsDirty = true;
    update();  // Trigger a redraw to reflect the camera changes
}

void GLWindow::wheelEvent(QWheelEvent *event)
{
    float delta = event->angleDelta().y() / 120.0f;  // 120 is the typical delta value for one notch of the wheel
    m_eye.setZ(m_eye.z() - 10 * delta);  // Zoom in or out based on wheel movement

    m_uniformsDirty = true;
    update();  // Trigger a redraw
}

void GLWindow::setZ(float v)
{
    m_eye.setZ(v);
    m_uniformsDirty = true;
    update();
}

void GLWindow::setR(float v)
{
    m_r = v;
    m_uniformsDirty = true;
    update();
}

void GLWindow::setR2(float v)
{
    m_r2 = v;
    m_uniformsDirty = true;
    update();
}