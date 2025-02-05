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

#include "depth_to_gpu.hpp"

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
    "layout(location = 1) in vec3 color;\n"  // Add color attribute

    "uniform mat4 projMatrix;\n"
    "uniform mat4 camMatrix;\n"
    "uniform mat4 worldMatrix;\n"
    "uniform vec2 translation;\n"

    "out vec3 fragColor;\n"

    "void main() {\n"
        "vec3 translatedVertex = vertex;\n"
        "translatedVertex.xy += translation;  // Apply the translation to x and y coordinates\n"
        //"texCoord = vec2(vertex.x / 784.0, vertex.y / 448.0);\n"
        "gl_Position = projMatrix * camMatrix * worldMatrix * vec4(translatedVertex, 1.0);\n"
        "fragColor = color;\n"  // Pass color
    "}\n";

static const char *fragmentShaderSource =
    "in vec3 fragColor;\n"
    "out vec4 fragColorOut;\n"

    "void main() {\n"
        "fragColorOut = vec4(fragColor, 1.0);\n"  // Use per-vertex color
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
    makeCurrent();

    // 1) Acquire OpenGL function pointers
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    if (!f) {
        qDebug() << "Failed to initialize OpenGL functions!";
        return;
    }

    // 2) Compile & link your shaders
    qDebug() << "Compiling and linking shaders...";
    m_program = new QOpenGLShaderProgram();
    m_program->addShaderFromSourceCode(
        QOpenGLShader::Vertex,
        versionedShaderCode(vertexShaderSource)  // e.g. "#version 330\n ... your VS code ..."
    );
    m_program->addShaderFromSourceCode(
        QOpenGLShader::Fragment,
        versionedShaderCode(fragmentShaderSource) // e.g. "#version 330\n ... your FS code ..."
    );
    if (!m_program->link()) {
        qDebug() << "Shader program link error:" << m_program->log();
        return;
    }
    qDebug() << "Shaders compiled and linked.";

    // 3) Bind the shader and retrieve uniform locations
    m_program->bind();
    m_projMatrixLoc  = m_program->uniformLocation("projMatrix");
    m_camMatrixLoc   = m_program->uniformLocation("camMatrix");
    m_worldMatrixLoc = m_program->uniformLocation("worldMatrix");
    m_translation    = m_program->uniformLocation("translation");

    // 4) Create & bind the VAO
    qDebug() << "Creating VAO...";
    m_vao = new QOpenGLVertexArrayObject();
    if (!m_vao->create()) {
        qDebug() << "Failed to create VAO!";
        return;
    }
    m_vao->bind();
    qDebug() << "VAO created and bound.";

    // 5) Create & bind the VBO
    qDebug() << "Creating VBO...";
    m_vbo = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    if (!m_vbo->create()) {
        qDebug() << "Failed to create VBO!";
        return;
    }
    m_vbo->bind();

    // 6) Allocate a large enough buffer for all possible points
    //    - We have 448 x 784 = 351,232 pixels max
    //    - Each pixel => (X,Y,Z) + (R,G,B) = 6 floats
    //    - So total floats = 351,232 * 6
    //    - We'll just allocate GPU memory once (no actual data yet)
    size_t maxWidth   = 784;
    size_t maxHeight  = 448;
    size_t maxPoints  = maxWidth * maxHeight;  // 351,232
    size_t floatsPerVertex = 6;                // X,Y,Z,R,G,B
    size_t totalFloats = maxPoints * floatsPerVertex;
    size_t bufferBytes = totalFloats * sizeof(GLfloat);

    qDebug() << "Allocating VBO with" << bufferBytes << "bytes...";
    m_vbo->allocate(nullptr, bufferBytes);  // allocate on GPU, no data yet

    // 7) Define the layout: interleaved (X,Y,Z,R,G,B)
    //    So each vertex has 6 floats => stride = 6*sizeof(GLfloat).
    GLsizei stride = 6 * sizeof(GLfloat);

    // -- Position attribute (location=0)
    f->glEnableVertexAttribArray(0);
    f->glVertexAttribPointer(
        0,                // index/location in shader
        3,                // 3 floats (X,Y,Z)
        GL_FLOAT,
        GL_FALSE,
        stride,
        reinterpret_cast<void*>(0) // offset 0 in each interleaved vertex
    );

    // -- Color attribute (location=1)
    f->glEnableVertexAttribArray(1);
    f->glVertexAttribPointer(
        1,                // index/location in shader
        3,                // 3 floats (R,G,B)
        GL_FLOAT,
        GL_FALSE,
        stride,
        reinterpret_cast<void*>(3 * sizeof(GLfloat)) // offset after X,Y,Z
    );

    // 8) Unbind VBO & VAO
    m_vbo->release();
    m_vao->release();

    // 9) Set up some default camera position or transformations
    m_eye = QVector3D(0, 0, -10.0f);
    // If you want to shift the point cloud so it’s centered:
    float centerX = (maxWidth  - 1) / 2.0f;
    float centerY = (maxHeight - 1) / 2.0f;
    QVector2D centeredTranslation(-centerX, -centerY);
    //m_program->setUniformValue(m_translation, centeredTranslation);

    // 10) Configure OpenGL states (depth test, cull face, etc.)
    qDebug() << "Enabling depth test and face culling...";
    f->glEnable(GL_DEPTH_TEST);
    f->glEnable(GL_CULL_FACE);

    // 11) Release the shader
    m_program->release();

    // 12) Done
    doneCurrent();
    qDebug() << "initializeGL() complete.";
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
    m_colors.clear();

    cv::cuda::GpuMat depthGPU;
    depthGPU.upload(newDepthMap);

    float cx = 4.0071597e+02;
    float cy = 2.1588844e+02;
    float fx = 1.1948323e+03;
    float fy = 1.1948323e+03;

    // 4) Create a GPU Mat for the point cloud result
    cv::cuda::GpuMat cloudGPU;

    // 5) Call the GPU function
    depthToCloudGPU(depthGPU, cloudGPU, fx, fy, cx, cy);

    // Now 'cloudGPU' is CV_32FC3, where each pixel is (X, Y, Z).

    // 6) (Optional) Download to CPU if you want to process or visualize in CPU
    cv::Mat cloudCPU;
    cloudGPU.download(cloudCPU);

    qDebug() << "Processing depth map...";

    std::vector<GLfloat> interleavedData;
    interleavedData.reserve(newDepthMap.total() * 6);  // 6 floats per pixel

    float scaleXY = 1.0f;
    float scaleZ  = 1.0f; // or 1.0f, or whatever

    for (int y = 0; y < newDepthMap.rows; ++y) {
        for (int x = 0; x < newDepthMap.cols; ++x) {
            // 1) Retrieve the 3D point (X, Y, Z)
            cv::Vec3f point = cloudCPU.at<cv::Vec3f>(y, x);

            // 2) Retrieve color from the RGB image
            cv::Vec3b colorBGR = newRGBImage.at<cv::Vec3b>(y, x);

            // 3) Push back (X, Y, Z) then (R, G, B)
            interleavedData.push_back(point[0] * scaleXY);                // X
            interleavedData.push_back(point[1] * scaleXY);                // Y
            interleavedData.push_back(point[2] * scaleZ);                // Z
            interleavedData.push_back(colorBGR[2] / 255.0f);    // R
            interleavedData.push_back(colorBGR[1] / 255.0f);    // G
            interleavedData.push_back(colorBGR[0] / 255.0f);    // B
        }
    }

    qDebug() << "Updating VBO...";
    m_vao->bind();  // ✅ Ensure VAO is bound before writing VBO
    m_vbo->bind();
    
    size_t bytesToWrite = interleavedData.size() * sizeof(GLfloat);
    m_vbo->write(0, interleavedData.data(), bytesToWrite);
    
    m_vbo->release();
    m_vao->release();  // ✅ Unbind VAO after updates

// 5) Save the current # of vertices for glDrawArrays
    m_numPoints = interleavedData.size() / 6;  // each vertex has 6 floats

    update(); // trigger a redraw
}



void GLWindow::resizeGL(int w, int h)
{
    m_proj.setToIdentity();
    //m_proj.perspective(45.0f, GLfloat(w) / h, 0.01f, 5000.0f);
    m_proj.perspective(75.0f, float(w) / float(h), 0.1f, 10000.0f);  // Increase far plane
    m_uniformsDirty = true;
}

void GLWindow::paintGL()
{
    QOpenGLExtraFunctions *f = QOpenGLContext::currentContext()->extraFunctions();

    f->glClearColor(0, 0, 0, 1);
    f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_program->bind();

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

    glPointSize(2.0f);
    m_vao->bind();  // Ensure the VAO is bound
    f->glDrawArrays(GL_POINTS, 0, m_numPoints);

    // matbe should call these too?!
    m_vao->release();
    m_program->release();
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