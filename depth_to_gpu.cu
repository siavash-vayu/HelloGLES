#include "depth_to_gpu.cuh"
#include "depth_to_gpu.hpp"

#include <cuda_runtime.h>
#include <opencv2/core/cuda.hpp>
#include <opencv2/cudaarithm.hpp>

// CUDA kernel definition
__global__ void depthToCloudKernel(cv::cuda::PtrStepSz<float> depth,
                                   cv::cuda::PtrStepSz<float3> cloud,
                                   float fx, float fy, float cx, float cy)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= depth.cols || y >= depth.rows)
        return;

    float z = depth(y, x);
    float X = (x - cx) * z / fx;
    float Y = (y - cy) * z / fy;

    cloud(y, x) = make_float3(X, Y, z);
}

// C++ function callable from .cpp files
void depthToCloudGPU(const cv::cuda::GpuMat& depthMap,
                     cv::cuda::GpuMat& pointCloud,
                     float fx, float fy, float cx, float cy)
{
    CV_Assert(depthMap.type() == CV_32FC1);

    pointCloud.create(depthMap.size(), CV_32FC3);

    dim3 block(16, 16);
    dim3 grid((depthMap.cols + block.x - 1) / block.x,
              (depthMap.rows + block.y - 1) / block.y);

    depthToCloudKernel<<<grid, block>>>(
        depthMap, 
        (cv::cuda::PtrStepSz<float3>)pointCloud, 
        fx, fy, cx, cy
    );

    cudaDeviceSynchronize();
}
