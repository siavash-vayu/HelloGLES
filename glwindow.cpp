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

#include <opencv2/opencv.hpp>


float depth_map[10][10] = {
    {0.63429755, 0.85441285, 0.08382889, 0.6956814,  0.35567918, 0.02570716, 0.00646774, 0.82127213, 0.9928988,  0.6829118},
    {0.43528765, 0.974004,   0.3529426,  0.96495193, 0.8339659,  0.04751923, 0.22605656, 0.89321274, 0.54584634, 0.12958233},
    {0.9751791,  0.17515804, 0.8769521,  0.91056365, 0.26910332, 0.6644882,  0.74858785, 0.5397522,  0.79052365, 0.12372913},
    {0.07933901, 0.3334724,  0.2735722,  0.4789211,  0.6905947,  0.7732844,  0.16543607, 0.9704664,  0.09590932, 0.03252332},
    {0.4811065,  0.85156703, 0.4849581,  0.7758822,  0.12576494, 0.43041402, 0.9402388,  0.5501022,  0.26642558, 0.6605646},
    {0.69177157, 0.76212347, 0.87042135, 0.74657637, 0.6746732,  0.86166763, 0.8833649,  0.5769891,  0.4859869,  0.08696652},
    {0.93936974, 0.39788204, 0.21829018, 0.95309,    0.11134953, 0.8063174,  0.00975959, 0.1693005,  0.30038393, 0.98534274},
    {0.10908719, 0.35996732, 0.68241537, 0.38501054, 0.7483173,  0.22079338, 0.09575351, 0.50698584, 0.19893615, 0.33322167},
    {0.7603783,  0.62417924, 0.9753153,  0.22238792, 0.7158476,  0.24180582, 0.880934,   0.28795084, 0.84798765, 0.54302096},
    {0.31233507, 0.32947558, 0.6116558,  0.7482836,  0.21618518, 0.59165317, 0.08508845, 0.6554845,  0.59711534, 0.0649782}
    };

std::vector<GLfloat> vertices;


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

    /*setMouseTracking(true);

    // Optionally grab mouse and keyboard focus
    grabMouse();
    grabKeyboard();*/

    /*QSequentialAnimationGroup *animGroup = new QSequentialAnimationGroup(this);
    animGroup->setLoopCount(-1);
    QPropertyAnimation *zAnim0 = new QPropertyAnimation(this, QByteArrayLiteral("z"));
    zAnim0->setStartValue(1.5f);
    zAnim0->setEndValue(10.0f);
    zAnim0->setDuration(2000);
    animGroup->addAnimation(zAnim0);
    QPropertyAnimation *zAnim1 = new QPropertyAnimation(this, QByteArrayLiteral("z"));
    zAnim1->setStartValue(10.0f);
    zAnim1->setEndValue(50.0f);
    zAnim1->setDuration(4000);
    zAnim1->setEasingCurve(QEasingCurve::OutElastic);
    animGroup->addAnimation(zAnim1);
    QPropertyAnimation *zAnim2 = new QPropertyAnimation(this, QByteArrayLiteral("z"));
    zAnim2->setStartValue(50.0f);
    zAnim2->setEndValue(1.5f);
    zAnim2->setDuration(2000);
    animGroup->addAnimation(zAnim2);
    animGroup->start();

    QPropertyAnimation* rAnim = new QPropertyAnimation(this, QByteArrayLiteral("r"));
    rAnim->setStartValue(0.0f);
    rAnim->setEndValue(360.0f);
    rAnim->setDuration(2000);
    rAnim->setLoopCount(-1);
    rAnim->start();

    animGroup->start();

    QTimer::singleShot(4000, this, &GLWindow::startSecondStage);*/
    /*QPropertyAnimation* rAnim = new QPropertyAnimation(this, QByteArrayLiteral("r"));
    rAnim->setStartValue(0.0f);
    rAnim->setEndValue(360.0f);
    rAnim->setDuration(2000);
    rAnim->setLoopCount(-1);
    rAnim->start();*/
}

void GLWindow::mousePressEvent(QMouseEvent *event)
{
    m_lastMousePosition = event->pos();
}

void GLWindow::mouseMoveEvent(QMouseEvent *event)
{
    int dx = event->x() - m_lastMousePosition.x();
    int dy = event->y() - m_lastMousePosition.y();

    m_yaw += dx * 0.5f;  // Adjust the sensitivity as needed
    m_pitch += dy * 0.5f;

    m_lastMousePosition = event->pos();

    m_uniformsDirty = true;
    update();  // Trigger a redraw
}

void GLWindow::mouseReleaseEvent(QMouseEvent *event)
{
    // No special action needed on release
}

void GLWindow::keyPressEvent(QKeyEvent *event)
{
    float step = 5.0;//0.5f;  // Adjust this step value as needed

    switch (event->key()) {
    case Qt::Key_W:
        m_eye.setZ(m_eye.z() - step);   // Move up
        break;
    case Qt::Key_S:
        m_eye.setZ(m_eye.z() + step);   // Move down
        break;
    case Qt::Key_A:
        m_eye.setX(m_eye.x() - step);  // Pan left
        break;
    case Qt::Key_D:
        m_eye.setX(m_eye.x() + step);  // Pan right
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
    m_eye.setZ(m_eye.z() - delta);  // Zoom in or out based on wheel movement

    m_uniformsDirty = true;
    update();  // Trigger a redraw
}

GLWindow::~GLWindow()
{
    makeCurrent();
    delete m_texture;
    delete m_program;
    delete m_vbo;
    delete m_vao;
}

void GLWindow::startSecondStage()
{
    QPropertyAnimation* r2Anim = new QPropertyAnimation(this, QByteArrayLiteral("r2"));
    r2Anim->setStartValue(0.0f);
    r2Anim->setEndValue(360.0f);
    r2Anim->setDuration(20000);
    r2Anim->setLoopCount(-1);
    r2Anim->start();
}

void GLWindow::setZ(float v)
{
    //m_eye.setZ(v);
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

static const char *vertexShaderSource =
    "layout(location = 0) in vec3 vertex;\n"
    "layout(location = 1) in vec3 normal;\n"

    "uniform mat4 projMatrix;\n"
    "uniform mat4 camMatrix;\n"
    "uniform mat4 worldMatrix;\n"

    "out vec2 texCoord;\n"

    "void main() {\n"
        "texCoord = vec2(vertex.x / 784.0, vertex.y / 448.0);\n"
        "gl_Position = projMatrix * camMatrix * worldMatrix * vec4(vertex, 1.0);\n"
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

void GLWindow::initializeGL()
{
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();

    if (m_texture) {
        delete m_texture;
        m_texture = 0;
    }

    m_eye = QVector3D(0, 0, 5.0f);  // Move the camera farther away along the z-axis
    
    //QImage img("../qtlogo.png");
    QImage img("../RectL.bmp");
    Q_ASSERT(!img.isNull());
    //m_texture = new QOpenGLTexture(img.scaled(784, 448).mirrored());
    m_texture = new QOpenGLTexture(img.scaled(784, 448));

    if (m_program) {
        delete m_program;
        m_program = 0;
    }
    m_program = new QOpenGLShaderProgram;
    // Prepend the correct version directive to the sources. The rest is the
    // same, thanks to the common GLSL syntax.
    m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, versionedShaderCode(vertexShaderSource));
    m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, versionedShaderCode(fragmentShaderSource));
    m_program->link();

    m_projMatrixLoc = m_program->uniformLocation("projMatrix");
    m_camMatrixLoc = m_program->uniformLocation("camMatrix");
    m_worldMatrixLoc = m_program->uniformLocation("worldMatrix");
    m_myMatrixLoc = m_program->uniformLocation("myMatrix");
    m_lightPosLoc = m_program->uniformLocation("lightPos");

    // Create a VAO. Not strictly required for ES 3, but it is for plain OpenGL.
    if (m_vao) {
        delete m_vao;
        m_vao = 0;
    }
    m_vao = new QOpenGLVertexArrayObject;
    if (m_vao->create())
        m_vao->bind();

    if (m_vbo) {
        delete m_vbo;
        m_vbo = 0;
    }
    m_program->bind();
    m_vbo = new QOpenGLBuffer;
    m_vbo->create();
    m_vbo->bind();

    cv::Mat depthMap = cv::imread("../Depth.exr", cv::IMREAD_UNCHANGED);

    std::cout << "depthMap.cols = " << depthMap.cols << std::endl;
    std::cout << "depthMap.rows = " << depthMap.rows << std::endl;

    float centerX = depthMap.cols / 2.0f;
    float centerY = depthMap.rows / 2.0f;


    float scaleFactor = 0.1f;  // Adjust this factor based on your depth map values
    float depthMult = 30.0f;

    for (int y = 0; y < depthMap.rows; ++y) {
        for (int x = 0; x < depthMap.cols; ++x) {
            float z = depthMap.at<float>(y, x);  // z-coordinate from the depth map
            vertices.push_back(static_cast<float>(x));  // x-coordinate
            vertices.push_back(static_cast<float>(y));  // y-coordinate
            vertices.push_back(z * depthMult);  // z-coordinate from the depth map

            // Normal vector - set to a default value if not calculated
            vertices.push_back(0.0f);  // nx
            vertices.push_back(0.0f);  // ny
            vertices.push_back(1.0f);  // nz
        }
    }

    //m_vbo->allocate(m_logo.constData(), m_logo.count() * sizeof(GLfloat));
    m_vbo->allocate(vertices.data(), vertices.size() * sizeof(GLfloat));
    f->glEnableVertexAttribArray(0);
    f->glEnableVertexAttribArray(1);
    f->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), 0);
    f->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat),
                             reinterpret_cast<void *>(3 * sizeof(GLfloat)));
    m_vbo->release();

    f->glEnable(GL_DEPTH_TEST);
    f->glEnable(GL_CULL_FACE);
}

void GLWindow::resizeGL(int w, int h)
{
    m_proj.setToIdentity();
    m_proj.perspective(45.0f, GLfloat(w) / h, 0.01f, 1000.0f);
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
        //wm.rotate(m_r, 1, 1, 0);
        //wm.rotate(m_r, 0, 1, 0);
        wm.rotate(m_yaw, 0, 1, 0);  // Rotate around y-axis based on yaw
        wm.rotate(m_pitch, 1, 0, 0);  // Rotate around x-axis based on pitch
        m_program->setUniformValue(m_worldMatrixLoc, wm);
    }

    f->glDrawArrays(GL_POINTS, 0, vertices.size() / 6);
}
