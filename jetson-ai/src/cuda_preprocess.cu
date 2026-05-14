
#include "cuda_preprocess.hpp"
#include <cuda_runtime.h>
#include <iostream>

__global__ void preprocessKernel(unsigned char* input, int total)
{
    int idx = threadIdx.x + blockIdx.x * blockDim.x;
    if (idx >= total) return;

    unsigned char v = input[idx];
    input[idx] = (unsigned char)min(255, v);
}

void launchPreprocessing()
{
    const int N = 1024;
    unsigned char* host = new unsigned char[N];
    for (int i = 0; i < N; ++i) host[i] = (unsigned char)(i % 256);

    unsigned char* dev = nullptr;
    cudaError_t err = cudaMalloc((void**)&dev, N);
    if (err != cudaSuccess)
    {
        std::cerr << "cudaMalloc failed: " << cudaGetErrorString(err) << std::endl;
        delete[] host;
        return;
    }

    cudaMemcpy(dev, host, N, cudaMemcpyHostToDevice);
    const int block = 256;
    const int grid = (N + block - 1) / block;
    preprocessKernel<<<grid, block>>>(dev, N);
    cudaDeviceSynchronize();

    cudaMemcpy(host, dev, N, cudaMemcpyDeviceToHost);
    cudaFree(dev);
    delete[] host;
}
