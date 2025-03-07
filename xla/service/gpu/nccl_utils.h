/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

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

#ifndef XLA_SERVICE_GPU_NCCL_UTILS_H_
#define XLA_SERVICE_GPU_NCCL_UTILS_H_

#if TENSORFLOW_USE_ROCM
#define __HIP_DISABLE_CPP_FUNCTIONS__
#endif

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "xla/executable_run_options.h"
#include "xla/service/collective_ops_utils.h"
#include "xla/service/global_device_id.h"
#include "xla/service/gpu/gpu_executable_run_options.h"
#include "xla/service/gpu/thunk.h"
#include "xla/service/lockable.h"
#include "xla/statusor.h"
#include "xla/xla_data.pb.h"
#include "tsl/lib/gtl/int_type.h"
#include "tsl/platform/logging.h"

// Common place for all collective thunks to include nccl/rccl headers.
#if TENSORFLOW_USE_ROCM
#include "rocm/rocm_config.h"
#if (TF_ROCM_VERSION >= 50200)
#include "rocm/include/rccl/rccl.h"
#else
#include "rocm/include/rccl.h"
#endif
#else
#include "third_party/nccl/nccl.h"
#endif

namespace xla {
namespace gpu {

ncclRedOp_t ToNcclReduction(ReductionKind kind);

absl::StatusOr<std::pair<ncclDataType_t, int>> ToNcclDataTypeAndCountMultiplier(
    PrimitiveType element_type, Thunk::Kind reduction_op);

bool IsGlobalNcclConfig();

absl::Status ToStatus(ncclResult_t s, const char* file, int64_t line,
                      const char* expr);

// Macros to return or warn on CUDA/NCCL errors.  (The same macro works for both
// NCCL and CUDA errors.)
//
// It's tempting to say these macros belong in an XLA header somewhere, but in
// practice we don't do much direct-to-CUDA-API stuff outside of this file.
#define XLA_CUDA_STATUS(expr) \
  xla::gpu::ToStatus(expr, __FILE__, __LINE__, #expr)

#define XLA_CUDA_RETURN_IF_ERROR(expr)      \
  do {                                      \
    absl::Status s = XLA_CUDA_STATUS(expr); \
    if (!s.ok()) {                          \
      return s;                             \
    }                                       \
  } while (0)

#define XLA_CUDA_WARN_IF_ERROR(expr)        \
  do {                                      \
    absl::Status s = XLA_CUDA_STATUS(expr); \
    if (!s.ok()) {                          \
      LOG(ERROR) << s.ToString();           \
    }                                       \
  } while (0)

size_t GetNumLocalParticipants(
    const std::vector<GlobalDeviceId>& participants,
    const std::vector<GlobalDeviceId>* local_devices);  // may be null

absl::StatusOr<const NcclUniqueIdCallback*> GetNcclUniqueIdCallback(
    const NcclUniqueIdCallback* unique_id_callback,  // may be null
    bool is_local);

TSL_LIB_GTL_DEFINE_INT_TYPE(OpId, int64_t);

struct NcclComm : public Lockable<ncclComm_t> {
  explicit NcclComm(ncclComm_t comm) : Lockable(comm) {}
};

absl::StatusOr<NcclComm::Lock> AcquireNcclComm(
    RunId run_id, OpId op_id, std::vector<GlobalDeviceId> participants,
    size_t num_local_participants,
    const NcclUniqueIdCallback& unique_id_callback, int32_t rank,
    int64_t stream_id, bool enable_clique_optimization);

}  // namespace gpu
}  // namespace xla

#endif  // XLA_SERVICE_GPU_NCCL_UTILS_H_
