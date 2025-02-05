// Copyright 2025 vayu
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     https://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <opencv2/core/cuda.hpp>

// Declare a normal function (not a kernel)
/**
 * \brief Convert a GPU depth map (CV_32FC1) into a GPU point cloud (CV_32FC3).
 * 
 * \param depthMap  [in]  GpuMat (CV_32FC1), size = (rows, cols)
 * \param pointCloud [out] GpuMat (CV_32FC3), same size as depthMap
 * \param fx, fy, cx, cy  [in] camera intrinsics
 */
void depthToCloudGPU(const cv::cuda::GpuMat& depthMap,
                     cv::cuda::GpuMat& pointCloud,
                     float fx, float fy, float cx, float cy);