/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

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

#ifndef XLA_SERVICE_GPU_KERNEL_THUNK_H_
#define XLA_SERVICE_GPU_KERNEL_THUNK_H_

#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "mlir/IR/Operation.h"  // from @llvm-project
#include "mlir/IR/Value.h"  // from @llvm-project
#include "xla/hlo/ir/hlo_instruction.h"
#include "xla/service/buffer_assignment.h"
#include "xla/service/gpu/buffer_allocations.h"
#include "xla/service/gpu/kernel_arguments.h"
#include "xla/service/gpu/kernels/custom_kernel.h"
#include "xla/service/gpu/launch_dimensions.h"
#include "xla/service/gpu/thunk.h"
#include "xla/status.h"
#include "xla/stream_executor/stream_executor.h"
#include "xla/types.h"  // IWYU pragma: keep

namespace xla {
namespace gpu {

class GpuExecutable;

// TODO(ezhulenev): Unify KernelThunk and CustomKernelThunk as they are very
// similar. XLA:GPU should use more of kernel loading APIs provided by
// StreamExecutor out of the box and less custom kernel loading solutions.
//
// Today KernelThunk is required for lowering to XLA runtime, and
// CustomKernelThunk is only supported for thunk execution.

//===----------------------------------------------------------------------===//
// KernelThunk
//===----------------------------------------------------------------------===//

// This class stores everything that StreamExecutor needs for launching a
// kernel. It implements the ExecuteOnStream interface for GpuExecutable to
// invoke the corresponding kernel.
//
// This is thread-compatible.
class KernelThunk : public Thunk {
 public:
  // Constructs a thunk for the given kernel.
  //
  // KernelThunk takes args as `BufferAllocation::Slice`s (wrapped in
  // `KernelArgument`s). Each slice directly corresponds to an argument or
  // output of the computation. Also, the values must correspond to each arg
  // directly, not to their base allocation (e.g. they can be the result of an
  // `mlir::memref::ViewOp`).
  KernelThunk(std::variant<mlir::Operation*, const HloInstruction*> op,
              std::string kernel_name,
              absl::Span<const KernelArgument> kernel_arguments,
              LaunchDimensions launch_dimensions, int64_t shmem_bytes);
  KernelThunk(const KernelThunk&) = delete;
  KernelThunk& operator=(const KernelThunk&) = delete;
  ~KernelThunk() override = default;

  std::string ToStringExtra(int indent) const override;

  Status Initialize(se::StreamExecutor* executor,
                    ExecutableSource src) override;
  Status ExecuteOnStream(const ExecuteParams& params) override;

  void ClearCompileTimeInfo() override {
    Thunk::ClearCompileTimeInfo();
    for (auto& value : values_) {
      value = nullptr;
    }
  }

  const std::vector<BufferAllocation::Slice>& arguments() const {
    return args_;
  }
  const std::vector<bool>& written() const { return written_; }

  const std::string& kernel_name() const { return kernel_name_; }
  const LaunchDimensions& launch_dimensions() const {
    return launch_dimensions_;
  }
  // The shared memory required by the kernel.
  int64_t shmem_bytes() const { return shmem_bytes_; }
  absl::Span<const mlir::Value> values() const { return values_; }

 private:
  // Buffer slices passed to the kernel as arguments.
  std::vector<BufferAllocation::Slice> args_;

  // args_[i] is written iff (written_[i] == true).
  std::vector<bool> written_;

  // Entry kernel name for the computation.
  const std::string kernel_name_;

  // The thread and block dimension used to launch the kernel.
  const LaunchDimensions launch_dimensions_;

  int64_t shmem_bytes_;

  // mlir::Value(s) corresponding to the buffer slice arguments.
  std::vector<mlir::Value> values_;

  // Loaded kernels for each `StreamExecutor`.
  mutable absl::Mutex mutex_;
  absl::flat_hash_map<se::StreamExecutor*, std::unique_ptr<se::Kernel>>
      kernel_cache_ ABSL_GUARDED_BY(mutex_);
};

//===----------------------------------------------------------------------===//
// CustomKernelThunk
//===----------------------------------------------------------------------===//

// CustomKernelThunk loads and executes kernels defined by a custom kernel
// (which in practice means hand written CUDA C++ kernel), instead of a kernel
// compiled by XLA and loaded from an executable source.
class CustomKernelThunk : public Thunk {
 public:
  CustomKernelThunk(const HloInstruction* instr,
                    kernel::CustomKernel custom_kernel,
                    absl::Span<const KernelArgument> kernel_arguments);

  std::string ToStringExtra(int indent) const override;

  Status Initialize(se::StreamExecutor* executor,
                    ExecutableSource src) override;
  Status ExecuteOnStream(const ExecuteParams& params) override;

 private:
  // Buffer slices passed to the kernel as arguments.
  std::vector<BufferAllocation::Slice> args_;

  // args_[i] is written iff (written_[i] == true).
  std::vector<bool> written_;

  // mlir::Value(s) corresponding to the buffer slice arguments.
  std::vector<mlir::Value> values_;

  kernel::CustomKernel custom_kernel_;

  // Loaded kernels for each `StreamExecutor`.
  mutable absl::Mutex mutex_;
  absl::flat_hash_map<se::StreamExecutor*, std::unique_ptr<se::Kernel>>
      kernel_cache_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace gpu
}  // namespace xla

#endif  // XLA_SERVICE_GPU_KERNEL_THUNK_H_
