/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#ifndef XLA_SERVICE_GPU_NCCL_ALL_GATHER_THUNK_H_
#define XLA_SERVICE_GPU_NCCL_ALL_GATHER_THUNK_H_

#include <vector>

#include "xla/service/collective_ops_utils.h"
#include "xla/service/gpu/nccl_collective_thunk.h"

namespace xla {
namespace gpu {

struct NcclAllGatherConfig {
  NcclCollectiveConfig config;
};

// Thunk that performs a NCCL-based All-Gather among CUDA GPU-based replicas.
class NcclAllGatherStartThunk : public NcclCollectiveThunk {
 public:
  NcclAllGatherStartThunk(ThunkInfo thunk_info,
                          mlir::lmhlo_gpu::AllGatherStartOp op,
                          std::vector<Buffer> buffers);

  static const char* GetHloOpName() { return "all-gather-start"; }

  static Status CheckImplementable(mlir::lmhlo_gpu::AllGatherStartOp op,
                                   int64_t replica_count,
                                   int64_t partition_count);
  static bool IsDegenerate(mlir::lmhlo_gpu::AllGatherStartOp op,
                           int64_t replica_count, int64_t partition_count);
  static CollectiveOpGroupMode GetGroupMode(
      mlir::lmhlo_gpu::AllGatherStartOp op);
  static constexpr bool IsAsync() { return true; }

  AsyncExecutor& async_executor() { return async_; }

 protected:
  const NcclCollectiveConfig& config() const override { return config_.config; }
  Status RunNcclCollective(const ExecuteParams& params,
                           ncclComm_t comm) override;

 private:
  Status RunAllGather(const ExecuteParams& params, se::Stream& stream,
                      ncclComm_t comm);

  const NcclAllGatherConfig config_;
  const std::vector<Buffer> buffers_;
  AsyncExecutor async_;
};

class NcclAllGatherDoneThunk : public NcclCollectiveDoneThunk {
 public:
  NcclAllGatherDoneThunk(ThunkInfo thunk_info,
                         NcclCollectiveThunk::AsyncExecutor& async);
};

Status RunAllGather(std::vector<DeviceBufferPair>& buffers, se::Stream& stream,
                    ncclComm_t comm);

}  // namespace gpu
}  // namespace xla

#endif  // XLA_SERVICE_GPU_NCCL_ALL_GATHER_THUNK_H_
