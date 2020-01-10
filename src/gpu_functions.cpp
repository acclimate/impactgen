/*
  Copyright (C) 2019 Sven Willner <sven.willner@pik-potsdam.de>

  This file is part of the Acclimate ImpactGen.

  Acclimate ImpactGen is free software: you can redistribute it and/or
  modify it under the terms of the GNU Affero General Public License
  as published by the Free Software Foundation, either version 3 of
  the License, or (at your option) any later version.

  Acclimate ImpactGen is distributed in the hope that it will be
  useful, but WITHOUT ANY WARRANTY; without even the implied warranty
  of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Affero General Public License for more details.

  You should have received a copy of the GNU Affero General Public
  License along with Acclimate ImpactGen.  If not, see
  <http://www.gnu.org/licenses/>.
*/

#include "gpu_functions.h"
#include <cassert>

namespace cudatools {

#if defined(USE_CUDA) && defined(__CUDACC__)

__global__ void reduce_blocks_gpu(ReductionBlock* reduction_blocks, int* reduction_block_indices, float* in, float* gpu_block_out, float* out, unsigned int n) {
    extern __shared__ float shared[];
    const auto gpu_thread_id = threadIdx.x;
    const auto gpu_block_first = blockIdx.x * blockDim.x;
    const auto i = gpu_block_first + gpu_thread_id;
    const auto reduction_block_index = reduction_block_indices[i];
    const auto& reduction_block = reduction_blocks[reduction_block_index];
    int first;
    int count;
    if (reduction_block.first < gpu_block_first) {
        first = gpu_block_first;
        count = fminf(blockDim.x, reduction_block.count + reduction_block.first - gpu_block_first);
    } else {
        first = reduction_block.first;
        count = fminf(blockDim.x + reduction_block.first - gpu_block_first, reduction_block.count);
    }

    shared[gpu_thread_id] = (i < n) ? in[i] : 0;
    __syncthreads();

    for (unsigned int offset = blockDim.x / 2; offset > 0; offset >>= 1) {
        if (i < first + offset && i + offset < first + count && gpu_thread_id + offset < blockDim.x) {
            shared[gpu_thread_id] += shared[gpu_thread_id + offset];
        }
        __syncthreads();
    }

    if (gpu_thread_id == 0) {
        gpu_block_out[blockIdx.x] = shared[gpu_thread_id];
    }
    if (i == reduction_block.first) {
        if (gpu_thread_id == 0) {
            out[reduction_block_index] = 0;
        } else {
            out[reduction_block_index] = shared[gpu_thread_id];
        }
    }
}

__global__ void reduce_blocks_rest_gpu(ReductionBlock* reduction_blocks, int* reduction_block_indices, float* gpu_block_out, float* out, unsigned int n) {
    const auto gpu_thread_id = threadIdx.x;
    const auto gpu_block_first = blockIdx.x * blockDim.x;
    const auto former_block_size = blockDim.x;
    const auto i = gpu_block_first + gpu_thread_id;
    const auto former_i = i * former_block_size;
    if (former_i < n) {
        const auto reduction_block_index = reduction_block_indices[former_i];
        const auto& reduction_block = reduction_blocks[reduction_block_index];
        if (former_i < reduction_block.first + former_block_size) {
            float tmp = 0;
            for (auto b = i; b * former_block_size < reduction_block.first + reduction_block.count; ++b) {
                tmp += gpu_block_out[b];
            }
            out[reduction_block_index] += tmp;
        }
    }
}

void check_for_gpu_error() {
    const auto err = cudaGetLastError();
    if (err != cudaSuccess) {
        throw std::runtime_error(cudaGetErrorString(err));
    }
}

BlockReducer::BlockReducer(const std::vector<int>& cumulative_reduction_block_sizes) {
    assert(!cumulative_reduction_block_sizes.empty());

    const auto total = cumulative_reduction_block_sizes[cumulative_reduction_block_sizes.size() - 1];

    std::vector<ReductionBlock> reduction_blocks_cpu(cumulative_reduction_block_sizes.size());
    std::vector<int> reduction_block_indices_cpu(total);
    int current_index = -1;
    int last_cumulative_block_size = 0;
    int current_cumulative_block_size = 0;
    for (int i = 0; i < total; ++i) {
        while (i == current_cumulative_block_size) {
            ++current_index;
            last_cumulative_block_size = current_cumulative_block_size;
            current_cumulative_block_size = cumulative_reduction_block_sizes[current_index];
            assert(current_cumulative_block_size >= last_cumulative_block_size);
            reduction_blocks_cpu[current_index] = {current_index, current_cumulative_block_size - last_cumulative_block_size};
        }
        reduction_block_indices_cpu[i] = current_index;
    }
    reduction_blocks.resize(cumulative_reduction_block_sizes.size());
    reduction_block_indices.resize(total);
    reduction_blocks.set(&reduction_blocks_cpu[0]);
    reduction_block_indices.set(&reduction_block_indices[0]);

    int grid_size;
    cudaOccupancyMaxPotentialBlockSizeVariableSMem(
        &grid_size, &gpu_block_size, reduce_blocks_gpu, [](int gpu_block_size) { return gpu_block_size * sizeof(float); }, total);
    check_for_gpu_error();

    const auto gpu_block_count = (total + gpu_block_size - 1) / gpu_block_size;
    gpu_block_out.resize(gpu_block_count);
}

void Blockreducer::reduce(cudatools::device_pointer<float> in, cudatools::device_pointer<float> out, unsigned int n) {
    assert(n <= total);
    const auto gpu_block_count = gpu_block_out.size();
    reduce_blocks_gpu<<<gpu_block_count, gpu_block_size, gpu_block_size * sizeof(float)>>>(reduction_blocks.pointer(), reduction_block_indices.pointer(), in,
                                                                                           gpu_block_out.pointer(), out, n);
    cudaDeviceSynchronize();
    reduce_rest_gpu<<<(gpu_block_count + gpu_block_size - 1) / gpu_block_size, gpu_block_size>>>(reduction_blocks.pointer(), reduction_block_indices.pointer(),
                                                                                                 gpu_block_out.pointer(), out, n);
    cudaDeviceSynchronize();
    check_for_gpu_error();
}

#endif

}  // namespace cudatools
