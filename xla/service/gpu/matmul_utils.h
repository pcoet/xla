/* Copyright 2022 The TensorFlow Authors. All Rights Reserved.

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

#ifndef XLA_SERVICE_GPU_MATMUL_UTILS_H_
#define XLA_SERVICE_GPU_MATMUL_UTILS_H_

#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "xla/autotuning.pb.h"
#include "xla/hlo/ir/hlo_instruction.h"
#include "xla/mlir_hlo/lhlo_gpu/IR/lhlo_gpu_ops.h"
#include "xla/service/gpu/backend_configs.pb.h"
#include "xla/service/gpu/ir_emission_utils.h"
#include "xla/shape.h"
#include "xla/statusor.h"
#include "xla/stream_executor/blas.h"
#include "xla/stream_executor/gpu/gpu_blas_lt.h"
#include "xla/types.h"
#include "xla/xla_data.pb.h"

#if TENSORFLOW_USE_ROCM
#include "rocm/rocm_config.h"
#endif

namespace xla {
namespace gpu {

// Ordered non-contracting dimensions for a dot instruction operand.
absl::StatusOr<std::vector<int64_t>> GetNonContractingDims(
    const Shape& shape, absl::Span<const int64_t> batch_dims,
    absl::Span<const int64_t> contracting_dims);

// Batch dimensions of an operand of a dot instruction.
// Just an unified accessor to lhs_batch_dimensions and rhs_batch_dimensions.
const tsl::protobuf::RepeatedField<int64_t>& BatchDimensionsForOperand(
    const HloInstruction& dot, int operand_number);

// Index of the only contracting dimension of dot instruction operand.
int64_t ContractingDimensionIndex(const HloInstruction& dot,
                                  int operand_number);

// Index of the only non-contracting dimension of dot instruction operand.
int64_t NonContractingDimensionIndex(const HloInstruction& dot,
                                     int operand_number);

// Normalize shape to (batch, rows, columns) logical dimensions.
absl::StatusOr<Shape> GetBatchRowColumnShape(
    const Shape& shape, absl::Span<const int64_t> batch_dims,
    absl::Span<const int64_t> row_dims, absl::Span<const int64_t> col_dims);

// GPU folding rule for the `TransposeFolding` pass.
absl::StatusOr<bool> CanFoldTransposeOperandIntoDot(const HloInstruction& dot,
                                                    int64_t operand_idx);

// extending plain MatrixLayout struct with creator functions
struct MatrixLayout : public se::gpu::MatrixLayout {
  // Returns the matrix layout for a logical shape (batch, rows, columns).
  static absl::StatusOr<MatrixLayout> For(const Shape& shape);
  // Returns the matrix layout with the given batch, row, col dimensions.
  static absl::StatusOr<MatrixLayout> For(const Shape& shape,
                                          absl::Span<const int64_t> batch_dims,
                                          absl::Span<const int64_t> row_dims,
                                          absl::Span<const int64_t> col_dims);
  // Returns the matrix layout for the output.
  static absl::StatusOr<MatrixLayout> For(const Shape& shape,
                                          size_t lhs_num_batch_dims,
                                          size_t lhs_num_row_dims,
                                          size_t rhs_num_batch_dims,
                                          size_t rhs_num_col_dims);
};

struct GemmConfig : public se::gpu::GemmConfig {
  // For legacy Gemm operations XLA:GPU allocates its own workspace and passes
  // it to all BLAS API calls.
  //
  // Size of the workspace based on NVIDIA recommendation:
  // https://docs.nvidia.com/cuda/cublas/#cublassetworkspace
  static constexpr int64_t kHopperWorkspace = 32 * 1024 * 1024;  // 32 MiB
  static constexpr int64_t kDefaultWorkspace = 4 * 1024 * 1024;  // 4 MiB

  static absl::StatusOr<GemmConfig> For(const HloInstruction* gemm);
  static absl::StatusOr<GemmConfig> For(mlir::lmhlo_gpu::GEMMOp op);

  static absl::StatusOr<GemmConfig> For(
      const Shape& lhs_shape, absl::Span<const int64_t> lhs_batch_dims,
      absl::Span<const int64_t> lhs_contracting_dims, const Shape& rhs_shape,
      absl::Span<const int64_t> rhs_batch_dims,
      absl::Span<const int64_t> rhs_contracting_dims, const Shape& output_shape,
      double alpha_real, double alpha_imag, double beta,
      std::optional<int64_t> algorithm, int64_t compute_precision, bool grad_x,
      bool grad_y);

  // As above with additional `c_shape` and `bias_shape_ptr` parameter, both
  // which are only necessarily for F8 gemms.
  static absl::StatusOr<GemmConfig> For(
      const Shape& lhs_shape, absl::Span<const int64_t> lhs_batch_dims,
      absl::Span<const int64_t> lhs_contracting_dims, const Shape& rhs_shape,
      absl::Span<const int64_t> rhs_batch_dims,
      absl::Span<const int64_t> rhs_contracting_dims, const Shape& c_shape,
      const Shape* bias_shape_ptr, const Shape& output_shape, double alpha_real,
      double alpha_imag, double beta, std::optional<int64_t> algorithm,
      int64_t compute_precision, bool grad_x, bool grad_y);

  template <typename CublasLtMatmulMaybeF8Op,
            typename = std::enable_if<
                std::is_same<CublasLtMatmulMaybeF8Op,
                             mlir::lmhlo_gpu::CublasLtMatmulOp>::value ||
                std::is_same<CublasLtMatmulMaybeF8Op,
                             mlir::lmhlo_gpu::CublasLtMatmulF8Op>::value>>
  static absl::StatusOr<GemmConfig> For(CublasLtMatmulMaybeF8Op op) {
    mlir::mhlo::DotDimensionNumbersAttr dot_dims = op.getDotDimensionNumbers();

    int64_t compute_precision = 0;  // Default
    if (op.getPrecisionConfig().has_value()) {
      auto precision_config = op.getPrecisionConfig();
      for (auto attr : precision_config.value()) {
        int64_t value = static_cast<int64_t>(
            attr.template cast<mlir::mhlo::PrecisionAttr>().getValue());
        if (value > compute_precision) {
          compute_precision = value;
        }
      }
    }

    Shape bias_shape;
    if (op.getBias() != nullptr) {
      bias_shape = GetShape(op.getBias());
    }
    return GemmConfig::For(
        GetShape(op.getA()), dot_dims.getLhsBatchingDimensions(),
        dot_dims.getLhsContractingDimensions(), GetShape(op.getB()),
        dot_dims.getRhsBatchingDimensions(),
        dot_dims.getRhsContractingDimensions(), GetShape(op.getC()),
        op.getBias() == nullptr ? nullptr : &bias_shape, GetShape(op.getD()),
        op.getAlphaReal().convertToDouble(),
        op.getAlphaImag().convertToDouble(), op.getBeta().convertToDouble(),
        op.getAlgorithm(), compute_precision, /*grad_x=*/false,
        /*grad_y=*/false);
  }

  struct DescriptorsTuple {
    se::gpu::MatrixDescriptor lhs;
    se::gpu::MatrixDescriptor rhs;
    se::gpu::OutputMatrixDescriptor output;
    bool operands_swapped;
  };
  absl::StatusOr<DescriptorsTuple> GetMatrixDescriptors(
      se::DeviceMemoryBase lhs_buf, se::DeviceMemoryBase rhs_buf,
      se::DeviceMemoryBase out_buf) const;
};

// Run the given GEMM instruction `gemm` subject to the configuration
// in `gemm_config` and the passed buffers.
//
// If `algorithm` is provided, it overrides the one specified in `config`.
absl::Status RunGemm(
    const GemmConfig& config, se::DeviceMemoryBase lhs_buffer,
    se::DeviceMemoryBase rhs_buffer, se::DeviceMemoryBase output_buffer,
    se::DeviceMemoryBase workspace_buffer, bool deterministic_ops,
    se::Stream* stream,
    std::optional<se::blas::AlgorithmType> algorithm = std::nullopt,
    se::blas::ProfileResult* profile_result = nullptr);

namespace gpublas_lt {

absl::StatusOr<bool> EpilogueAddsVectorBias(
    GemmBackendConfig_Epilogue epilogue);
absl::StatusOr<bool> EpilogueHasAuxiliaryOutput(
    GemmBackendConfig_Epilogue epilogue);

absl::StatusOr<se::gpu::BlasLt::Epilogue> AsBlasLtEpilogue(
    mlir::lmhlo_gpu::CublasLtMatmulEpilogue epilogue);

}  // namespace gpublas_lt

// We should use this in code instead of AutotuneResult::TritonGemmKey.
// This has some advantages, for example it can be used in hashmaps.
struct TritonGemmConfig {
  struct ClusterDims {
    constexpr ClusterDims() = default;
    constexpr ClusterDims(int x, int y, int z) : x(x), y(y), z(z) {}
    int x = 1;
    int y = 1;
    int z = 1;
  };

  constexpr TritonGemmConfig() = default;
  constexpr TritonGemmConfig(int block_m, int block_n, int block_k, int split_k,
                             int num_stages, int num_warps, int num_ctas = 1,
                             ClusterDims cluster_dims = ClusterDims(1, 1, 1),
                             bool enable_warp_specialization = false)
      : block_m(block_m),
        block_n(block_n),
        block_k(block_k),
        split_k(split_k),
        num_stages(num_stages),
        num_warps(num_warps),
        num_ctas(num_ctas),
        cluster_dims(cluster_dims),
        enable_warp_specialization(enable_warp_specialization) {}
  int block_m = 0;
  int block_n = 0;
  int block_k = 0;
  int split_k = 0;
  int num_stages = 0;
  int num_warps = 0;
  int num_ctas = 1;
  ClusterDims cluster_dims;
  bool enable_warp_specialization = false;

 private:
  auto ToTuple() const {
    return std::make_tuple(block_m, block_n, block_k, split_k, num_stages,
                           num_warps, num_ctas, cluster_dims.x, cluster_dims.y,
                           cluster_dims.z, enable_warp_specialization);
  }

 public:
  static TritonGemmConfig FromProto(const AutotuneResult::TritonGemmKey& proto);
  AutotuneResult::TritonGemmKey ToProto() const;

  std::string ToString() const;

  bool operator==(const TritonGemmConfig& other) const {
    return ToTuple() == other.ToTuple();
  }

  bool operator<(const TritonGemmConfig& other) const {
    return ToTuple() < other.ToTuple();
  }

  template <typename H>
  friend H AbslHashValue(H h, const TritonGemmConfig& config) {
    return H::combine(std::move(h), config.ToTuple());
  }
};

}  // namespace gpu
}  // namespace xla

#endif  // XLA_SERVICE_GPU_MATMUL_UTILS_H_
