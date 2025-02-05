#include <QGuiApplication>
#include <QSurfaceFormat>
#include <QTimer>
#include <QOpenGLContext>

#include "glwindow.h"

#include <opencv2/opencv.hpp>
#include <stdexcept>

#include <opencv2/opencv.hpp>

cv::Mat resizeAndPad(const cv::Mat& src) {
    int target_width = 448;
    int target_height = 784;

    // Compute new size while maintaining aspect ratio
    double aspect_ratio = static_cast<double>(src.cols) / src.rows;
    int new_width = target_width;
    int new_height = static_cast<int>(new_width / aspect_ratio);

    // Resize the image
    cv::Mat resized;
    cv::resize(src, resized, cv::Size(new_width, new_height));

    // Create a new image with the target size, filled with black (or any color)
    cv::Mat output(target_height, target_width, src.type(), cv::Scalar::all(0));

    // Compute the padding
    int top = (target_height - new_height) / 2;
    int bottom = target_height - new_height - top;

    // Copy the resized image into the center of the output image
    resized.copyTo(output(cv::Rect(0, top, new_width, new_height)));

    return output;
}



int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QSurfaceFormat fmt;
    fmt.setDepthBufferSize(24);

    // Request OpenGL 3.3 core or OpenGL ES 3.0.
    if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGL) {
        qDebug("Requesting 3.3 core context");
        fmt.setVersion(3, 3);
        fmt.setProfile(QSurfaceFormat::CoreProfile);
    } else {
        qDebug("Requesting 3.0 context");
        fmt.setVersion(3, 0);
    }

    QSurfaceFormat::setDefaultFormat(fmt);

    GLWindow glWindow;
    
    glWindow.showMaximized();

    cv::Mat depth_1 = cv::imread("../NFOV/boston_narrow_base/Depth_RAW.exr", cv::IMREAD_UNCHANGED);
    cv::Mat rgb_1 = cv::imread("../NFOV/boston_narrow_base/RectL.bmp", cv::IMREAD_UNCHANGED);

    cv::Mat depth_resized;
    cv::Mat rgb_resized;

    cv::resize(depth_1, depth_resized, cv::Size(784, 448));
    cv::resize(rgb_1, rgb_resized, cv::Size(784, 448));

    // Call updateFrame after the window is shown, ensuring the OpenGL context is ready
    QTimer::singleShot(2000, [&glWindow, depth_resized, rgb_resized]() {
        glWindow.updateFrame(depth_resized,
                             rgb_resized);
    });

    cv::Mat depth_2 = cv::imread("../NFOV/sikorsky/Depth_RAW.exr", cv::IMREAD_UNCHANGED);
    cv::Mat rgb_2 = cv::imread("../NFOV/sikorsky/RectL.bmp", cv::IMREAD_UNCHANGED);

    cv::Mat depth_resized_2;
    cv::Mat rgb_resized_2;

    cv::resize(depth_2, depth_resized_2, cv::Size(784, 448));
    cv::resize(rgb_2, rgb_resized_2, cv::Size(784, 448));

    // Call updateFrame after the window is shown, ensuring the OpenGL context is ready
    QTimer::singleShot(4000, [&glWindow, depth_resized_2, rgb_resized_2]() {
        glWindow.updateFrame(depth_resized_2,
                             rgb_resized_2);
    });

    return app.exec();
}
