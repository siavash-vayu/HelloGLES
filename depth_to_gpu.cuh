#pragma once

#include <opencv2/core/cuda.hpp>
#include <opencv2/cudaarithm.hpp>
#include <opencv2/highgui.hpp>
#include <iostream>

/**
 * CUDA kernel that unprojects a depth map into a 3D point cloud.
 *   depth(y, x) : float depth at pixel (x, y).
 *   cloud(y, x) : float3 for the output 3D point (X, Y, Z) at (x, y).
 */
__global__ void depthToCloudKernel(cv::cuda::PtrStepSz<float> depth,
                                   cv::cuda::PtrStepSz<float3> cloud,
                                   float fx, float fy, float cx, float cy);