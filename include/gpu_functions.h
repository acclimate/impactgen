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

#include "cudatools.h"

namespace cudatools {

struct ReductionBlock {
    int first = 0;
    int count = 0;
};

class BlockReducer {
  protected:
    int gpu_block_size;
    cudatools::vector<float, true> gpu_block_out;              // size: gpu_block_count
    cudatools::vector<ReductionBlock, true> reduction_blocks;  // size: reduction_block_count
    cudatools::vector<int, true> reduction_block_indices;      // size: total

  public:
    BlockReducer(const std::vector<int>& cumulative_reduction_block_sizes);
    void reduce(cudatools::device_pointer<float> in, cudatools::device_pointer<float> out, unsigned int n);
};

}  // namespace cudatools
