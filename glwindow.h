#ifndef GLWIDGET_H
#define GLWIDGET_H

#include <QOpenGLWindow>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QMatrix4x4>
#include <QVector3D>
#include "../hellogl2/logo.h"

QT_BEGIN_NAMESPACE

class QOpenGLTexture;
class QOpenGLShaderProgram;
class QOpenGLBuffer;
class QOpenGLVertexArrayObject;

QT_END_NAMESPACE

class GLWindow : public QOpenGLWindow
{
    Q_OBJECT
    Q_PROPERTY(float z READ z WRITE setZ)
    Q_PROPERTY(float r READ r WRITE setR)
    Q_PROPERTY(float r2 READ r2 WRITE setR2)

public:
    GLWindow();
    ~GLWindow();

    void initializeGL();
    void resizeGL(int w, int h);
    void paintGL();

    float z() const { return m_eye.z(); }
    void setZ(float v);

    float r() const { return m_r; }
    void setR(float v);
    float r2() const { return m_r2; }
    void setR2(float v);
private slots:
    void startSecondStage();

protected:
    // Event handlers for mouse interaction
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    
private:
    QOpenGLTexture *m_texture;
    QOpenGLShaderProgram *m_program;
    QOpenGLBuffer *m_vbo;
    QOpenGLVertexArrayObject *m_vao;
    Logo m_logo;
    int m_projMatrixLoc;
    int m_camMatrixLoc;
    int m_worldMatrixLoc;
    int m_myMatrixLoc;
    int m_lightPosLoc;
    QMatrix4x4 m_proj;
    QMatrix4x4 m_world;
    QVector3D m_eye;
    QVector3D m_target;
    bool m_uniformsDirty;
    float m_r;
    float m_r2;

    QPoint m_lastMousePosition;
    float m_yaw = 0.0f;  // Rotation around the y-axis
    float m_pitch = 0.0f;  // Rotation around the x-axis
    bool m_mousePressed;
};

#endif
