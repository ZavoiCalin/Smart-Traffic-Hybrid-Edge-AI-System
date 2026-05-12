
#include "cuda_preprocess.hpp"
#include <cuda_runtime.h>

__global__ void preprocessKernel(unsigned char* input)
{
    int idx = threadIdx.x + blockIdx.x * blockDim.x;
}

void launchPreprocessing()
{
}
