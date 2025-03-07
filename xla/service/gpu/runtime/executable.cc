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

#include "xla/service/gpu/runtime/executable.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "absl/status/status.h"
#include "xla/mlir/runtime/transforms/compilation_pipeline_gpu.h"
#include "xla/runtime/executable.h"
#include "xla/runtime/jit_executable.h"
#include "xla/service/gpu/non_atomically_upgradeable_rw_lock.h"
#include "xla/service/gpu/runtime/cholesky.h"
#include "xla/service/gpu/runtime/concurrent_region.h"
#include "xla/service/gpu/runtime/conv.h"
#include "xla/service/gpu/runtime/conv_reorder.h"
#include "xla/service/gpu/runtime/cub_sort.h"
#include "xla/service/gpu/runtime/custom_call.h"
#include "xla/service/gpu/runtime/custom_call_registry.h"
#include "xla/service/gpu/runtime/fft.h"
#include "xla/service/gpu/runtime/fused_attention.h"
#include "xla/service/gpu/runtime/gemm.h"
#include "xla/service/gpu/runtime/gpublas_lt_matmul.h"
#include "xla/service/gpu/runtime/graph_launch.h"
#include "xla/service/gpu/runtime/io_feed.h"
#include "xla/service/gpu/runtime/memcpy.h"
#include "xla/service/gpu/runtime/memset.h"
#include "xla/service/gpu/runtime/norm.h"
#include "xla/service/gpu/runtime/send_recv.h"
#include "xla/service/gpu/runtime/stream_synchronization.h"
#include "xla/service/gpu/runtime/support.h"
#include "xla/service/gpu/runtime/topk.h"
#include "xla/service/gpu/runtime/tracing.h"
#include "xla/service/gpu/thunk.h"
#include "xla/service/service_executable_run_options.h"
#include "xla/service/stream_pool.h"
#include "xla/statusor.h"
#include "xla/stream_executor/stream.h"
#include "tsl/protobuf/dnn.pb.h"

namespace xla {
namespace gpu {

using ::xla::runtime::CustomCallAttrEncodingSet;
using ::xla::runtime::DirectCustomCallRegistry;
using ::xla::runtime::Executable;
using ::xla::runtime::JitExecutable;
using ::xla::runtime::Tagged;
using ::xla::runtime::TypeIDNameRegistry;

using ::xla::runtime::CustomCall;
using ::xla::runtime::DiagnosticEngine;
using ::xla::runtime::ExportModules;

void RegisterXlaGpuRuntimeCustomCalls(DirectCustomCallRegistry& registry) {
  // Register custom calls from a static XLA:GPU registry.
  RegisterDirectCustomCalls(registry);

  // Register builtin XLA:GPU custom calls (aka GPU runtime).
  RegisterKernelLaunchCustomCalls(registry);
  RegisterTracingCustomCalls(registry);
  RegisterFftCustomCalls(registry);
  RegisterCholeskyCustomCalls(registry);
  RegisterCollectiveCustomCalls(registry);
  RegisterGemmCustomCalls(registry);
  RegisterConvCustomCalls(registry);
  RegisterConvReorderCustomCalls(registry);
  RegisterMemcpyCustomCalls(registry);
  RegisterIoFeedCustomCalls(registry);
  RegisterMemsetCustomCalls(registry);
  RegisterSendRecvCustomCalls(registry);

#if GOOGLE_CUDA || TF_HIPBLASLT
  RegisterMatmulCustomCalls(registry);
#endif  // GOOGLE_CUDA || TENSORFLOW_USE_ROCM
#if GOOGLE_CUDA
  RegisterNormCustomCalls(registry);
  RegisterFusedAttentionCustomCalls(registry);
  RegisterFusedAttentionBackwardCustomCalls(registry);
#endif  // GOOGLE_CUDA
#if GOOGLE_CUDA || TENSORFLOW_USE_ROCM
  // Graph launch kernels depend on Cuda Graph API.
  RegisterGraphLaunchCustomCalls(registry);
  RegisterConcurrentRegionCustomCalls(registry);
  RegisterStreamSynchronizationCustomCalls(registry);
  RegisterCubSortCustomCalls(registry);
  RegisterXlaClassicCustomCalls(registry);
  RegisterTopkCustomCall(registry);
#endif  // GOOGLE_CUDA || TENSORFLOW_USE_ROCM
}

void RegisterXlaGpuTypeIdNames(TypeIDNameRegistry& registry) {
  registry.Register<Tagged<se::dnn::ActivationMode>>(
      "__type_id_se_dnn_activation");
  registry.Register<Tagged<DotDimensionNumbers>>(
      "__type_id_dot_dimension_numbers");
  registry.Register<Tagged<se::fft::Type>>("__type_id_se_fft_type");

  RegisterTracingTypeIdNames(registry);
  RegisterConvTypeIdNames(registry);
  RegisterSendRecvTypeIdNames(registry);

#if GOOGLE_CUDA || TF_HIPBLASLT
  registry.Register<Tagged<se::gpu::BlasLt::Epilogue>>(
      "__type_id_se_gpublas_lt_epilogue");
  RegisterFusedAttentionTypeIdNames(registry);
  RegisterNormTypeIdNames(registry);
#endif  // GOOGLE_CUDA || TF_HIPBLASLT
}

void RegisterXlaGpuAttrEncoding(CustomCallAttrEncodingSet& encoding) {
  PopulateConvAttrEncoding(encoding);
  PopulateFftAttrEncoding(encoding);
  PopulateDotDimsAttrEncoding(encoding);
  PopulateSendRecvAttrEncoding(encoding);

#if GOOGLE_CUDA || TF_HIPBLASLT
  PopulateCublasLtMatmulAttrEncoding(encoding);
  PopulateFusedAttentionAlgorithmConfigAttrEncoding(encoding);
  PopulateFusedAttentionForwardDAGSignatureAttrEncoding(encoding);
  PopulateFusedAttentionBackwardDAGSignatureAttrEncoding(encoding);
  PopulateNormAlgorithmConfigAttrEncoding(encoding);
#endif  // GOOGLE_CUDA || TF_HIPBLASLT
}

//===----------------------------------------------------------------------===//

// Executable can have only one "main" function and only graph capture function.
static int64_t GetNumGraphs(const runtime::Executable& executable) {
  return executable.num_functions() - 1;
}

GpuRuntimeExecutable::GpuRuntimeExecutable(
    std::string module_name, std::vector<int64_t> buffer_sizes,
    std::vector<std::vector<int64_t>> allocation_indices,
    std::unique_ptr<JitExecutable> jit_executable, DebugOptions debug_options,
    ModulesState modules_state)
    : module_name_(std::move(module_name)),
      buffer_sizes_(std::move(buffer_sizes)),
      allocation_indices_(std::move(allocation_indices)),
      executable_(std::move(jit_executable)),
      debug_options_(std::move(debug_options)),
#if GOOGLE_CUDA || TENSORFLOW_USE_ROCM
      graph_instances_(module_name_, GetNumGraphs(executable())),
#endif  // GOOGLE_CUDA || TENSORFLOW_USE_ROCM
      modules_state_(std::move(modules_state)) {
  ExportModules(dynamic_custom_calls_);  // export runtime modules
}

GpuRuntimeExecutable::GpuRuntimeExecutable(
    std::string module_name, std::vector<int64_t> buffer_sizes,
    std::vector<std::vector<int64_t>> allocation_indices,
    std::unique_ptr<Executable> aot_executable, DebugOptions debug_options,
    ModulesState modules_state)
    : module_name_(std::move(module_name)),
      buffer_sizes_(std::move(buffer_sizes)),
      allocation_indices_(std::move(allocation_indices)),
      executable_(std::move(aot_executable)),
      debug_options_(std::move(debug_options)),
#if GOOGLE_CUDA || TENSORFLOW_USE_ROCM
      graph_instances_(module_name_, GetNumGraphs(executable())),
#endif  // GOOGL_CUDA || TENSORFLOW_USE_ROCM
      modules_state_(std::move(modules_state)) {
  ExportModules(dynamic_custom_calls_);  // export runtime modules
}

//===----------------------------------------------------------------------===//
// Compile Xla program lowered to runtime dialects to Gpu runtime executable.
//===----------------------------------------------------------------------===//

/*static*/ absl::StatusOr<std::unique_ptr<GpuRuntimeExecutable>>
GpuRuntimeExecutable::Create(std::string module_name,
                             std::unique_ptr<GpuRuntimeProgram> program) {
  // Options for the default XLA Runtime compilation pipeline.
  runtime::CompilationPipelineOptions copts;

  // Populate mapping from XLA (SE) enums/structs type id to symbol names.
  copts.populate_type_id_names = RegisterXlaGpuTypeIdNames;

  // For passing LMHLO attributes as XLA (SE) enums/structs to custom calls.
  copts.populate_attr_encodings = RegisterXlaGpuAttrEncoding;

  // Options for constructing XLA runtime JitExecutable.
  JitExecutable::Options opts;
  opts.specialization = JitExecutable::Specialization::kDisabled;
  opts.compiler.verification_level =
      program->debug_options.xla_gpu_llvm_verification_level();
  opts.compiler.register_dialects =
      runtime::RegisterDefaultXlaGpuRuntimeDialects;

  // Register XLA Gpu runtime custom calls with the linker.
  opts.compiler.symbols_binding = runtime::ToSymbolsBinding(
      RegisterXlaGpuRuntimeCustomCalls, RegisterXlaGpuTypeIdNames);

  // We just use the default compilation pipeline provided by the XLA runtime.
  // Alternatively instead of having a separate Xla Runtime program (LMHLO
  // lowered to canonical dialects), we can assemble a pipeline that will
  // compile starting from the LMHLO dialect. However this intermediate step
  // helps with debugging, by materializing IR with XLA runtime custom calls.
  opts.compiler.create_compilation_pipeline =
      [copts](xla::runtime::PassManager& passes) {
        runtime::CreateDefaultXlaGpuRuntimeCompilationPipeline(passes, copts);
      };

  // Do not run expensive optimization passes because we do not expect any
  // non-trivial host code in XLA:GPU host executables.
  opts.compiler.jit_code_opt_level = llvm::CodeGenOptLevel::None;

  // Instantiate new JitExecutable from the MLIR source.
  auto jit_executable =
      JitExecutable::Instantiate(program->module, program->entry_point, opts);
  if (!jit_executable.ok())
    return InternalError("Failed to compile XLA Runtime program: %s",
                         jit_executable.status().message());

  // Instantiate state for all registered runtime modules.
  auto modules_state = ModulesState::Instantiate();
  if (!modules_state.ok())
    return InternalError("Failed to instantiate modules state: %s",
                         modules_state.status().message());

  return std::unique_ptr<GpuRuntimeExecutable>(new GpuRuntimeExecutable(
      std::move(module_name), std::move(program->buffer_sizes),
      std::move(program->allocation_indices),
      std::make_unique<JitExecutable>(std::move(*jit_executable)),
      std::move(program->debug_options), std::move(*modules_state)));
}

//===----------------------------------------------------------------------===//
// Constructs Gpu runtime executable from AOT compiled runtime artifact.
//===----------------------------------------------------------------------===//

/*static*/ absl::StatusOr<std::unique_ptr<GpuRuntimeExecutable>>
GpuRuntimeExecutable::Create(
    std::string module_name, std::vector<int64_t> buffer_sizes,
    std::vector<std::vector<int64_t>> allocation_indices, Executable executable,
    DebugOptions debug_options) {
  // Instantiate state for all registered runtime modules.
  auto modules_state = ModulesState::Instantiate();
  if (!modules_state.ok())
    return InternalError("Failed to instantiate modules state: %s",
                         modules_state.status().message());

  return std::unique_ptr<GpuRuntimeExecutable>(new GpuRuntimeExecutable(
      std::move(module_name), std::move(buffer_sizes),
      std::move(allocation_indices),
      std::make_unique<Executable>(std::move(executable)),
      std::move(debug_options), std::move(*modules_state)));
}

//===----------------------------------------------------------------------===//
// Executes with the given buffer arguments.
//===----------------------------------------------------------------------===//

static runtime::AsyncTaskRunner* NoAsyncTaskRunner() {
  return reinterpret_cast<runtime::AsyncTaskRunner*>(0XDEADBEEF);
}

// TODO(ezhulenev): We rely on implementation details of passing memrefs to the
// compiled kernel. We should have a nicer API to do this, without creating a
// vector of temporary MemrefDesc for passing operands.
static void InitializeCallFrame(runtime::Executable::CallFrame& call_frame,
                                const BufferAllocations& buffer_allocations,
                                absl::Span<const int64_t> buffer_sizes,
                                llvm::SmallVectorImpl<void*>& ptrs) {
  size_t num_allocations = buffer_allocations.size();
  assert(ptrs.empty() && "pointers storage must be empty");
  ptrs.resize_for_overwrite(num_allocations);

  // Each buffer allocation passed as 1d memref to the compiled function:
  //   {basePtr, dataPtr, offset, [sizes, ...], [strides, ...]}
  size_t num_args_ptrs = 1 + num_allocations * 5;
  call_frame.args.resize_for_overwrite(num_args_ptrs);

  // Pass pointers to these constants as a memref offset and stride.
  static int64_t zero = 0;
  static int64_t one = 1;
  void* offset = &zero;
  void* stride = &one;

  // Add a placeholder for the kernel context as the first argument.
  call_frame.args[0] = nullptr;

  // Initialize arguments for the buffer operands.
  for (unsigned i = 0; i < num_allocations; ++i) {
    void* data = &(ptrs[i] = buffer_allocations.GetDeviceAddress(i).opaque());
    void* size = const_cast<int64_t*>(&buffer_sizes[i]);
    unsigned idx = 1 + i * 5;
    call_frame.args[idx + 0] = data;
    call_frame.args[idx + 1] = data;
    call_frame.args[idx + 2] = offset;
    call_frame.args[idx + 3] = size;
    call_frame.args[idx + 4] = stride;
  }
}

absl::Status GpuRuntimeExecutable::Execute(
    const ServiceExecutableRunOptions* run_options, const std::string& asm_text,
    const std::vector<uint8_t>& binary,
    const BufferAllocations& buffer_allocations,
    NonAtomicallyUpgradeableRWLock& gpu_lock,
    const BufferAllocation* temp_alloc) {
  // We pass a pointer to the executable through UserData, so that we can
  // get access to other exported functions from custom call handlers.
  runtime::Executable& executable = this->executable();

  // Pack buffer allocations as executable arguments. It is guaranteed that
  // the compiled function will make a copy of all arguments and will write all
  // results after the call to `Execute` completes, so it is safe to keep them
  // on the stack.
  runtime::Executable::CallFrame call_frame;

  llvm::SmallVector<void*, 16> ptrs;  // storage for device address pointers
  InitializeCallFrame(call_frame, buffer_allocations, buffer_sizes_, ptrs);

  // Check that initialized call frame is compatible with the executable
  // entry point signature, otherwise compiled executable can read memory out of
  // arguments bounds and crash with a segfault.
  const runtime::FunctionType& signature = executable.signature();
  if (signature.num_operands() != buffer_allocations.size())
    return InternalError("Expected %d arguments but got %d buffer allocations",
                         signature.num_operands(), buffer_allocations.size());

  for (unsigned i = 0; i < executable.signature().num_operands(); ++i) {
    auto* memref = llvm::dyn_cast<runtime::MemrefType>(signature.operand(i));
    if (!memref) return InvalidArgument("Expected memref as %d-th argument", i);

    if (memref->rank() != 1 || memref->sizes()[0] != buffer_sizes_[i])
      return InvalidArgument("Expected a buffer of size %d but got %d",
                             memref->sizes()[0], buffer_sizes_[i]);
  }

  // XLA Runtime executables do not return any values.
  runtime::NoResultConverter converter;

  // Get the async communications stream for async collectives.
  se::StreamExecutor* executor = run_options->stream()->parent();
  se::StreamPriority stream_priority = se::StreamPriority::Default;
  if (debug_options_.xla_gpu_enable_highest_priority_async_stream()) {
    stream_priority = se::StreamPriority::Highest;
  }

  // Create the needed streams to support NcclCollectiveThunk.
  //
  // Calling BorrowStream multiple times doesn't work as intended, see
  // b/293945751.
  absl::InlinedVector<se::Stream*, kAsyncStreamTotal> async_comm_streams(
      kAsyncStreamTotal, nullptr);
  absl::StatusOr<std::vector<StreamPool::Ptr>> streams =
      run_options->BorrowStreams(executor->device_ordinal(), kAsyncStreamTotal,
                                 stream_priority);
  if (streams.ok()) {
    for (int64_t i = 0; i < kAsyncStreamTotal; ++i) {
      async_comm_streams[i] = streams->at(i).get();
    }
  }

  // Async Collectives support and Send/Recv events instantiated for each Gpu
  // executable run, so that concurrent executions can run independently using a
  // separate set of events for communication.
  AsyncCollectivesSupport async_collectives(async_comm_streams);
  SendRecvEvents send_recv_events;

  // Always pass in the temp buffer, even if it is null, to accommodate the
  // 0-sized buffer corner case.
  se::DeviceMemoryBase temp_buffer;
  if (temp_alloc)
    temp_buffer = buffer_allocations.GetDeviceAddress(temp_alloc->index());

  // State cached separately for each stream executor.
  StreamExecutorKernels::Snapshot kernels = gpu_kernels_(executor)->snapshot();
  StreamExecutorConvRunners::Snapshot conv_runners =
      conv_runners_(executor)->snapshot();

#if GOOGLE_CUDA || TENSORFLOW_USE_ROCM
  std::shared_ptr<StreamExecutorGraphInstances> executor_graphs =
      graph_instances_(executor);

  StreamExecutorGraphInstances::Snapshot graph_instances =
      executor_graphs->snapshot();
  CapturedFunctionExecutionCount::Snapshot execution_count =
      captured_function_counts_(executor)->snapshot();
#endif  // GOOGLE_CUDA || TENSORFLOW_USE_ROCM

  // Kernels in concurrent regions should be launched on borrowed stream, so
  // that the cuda graph won't record dependencies between kernels.
  // This state stores if the kernel being run is in a concurrent region and
  // the borrowed streams for executing kernels in concurrent regions.
  ConcurrentRegionStatus concurrent_region_status(run_options);

  // State cached globally for gpu executable.
  GemmConfigs::Snapshot gemm_configs = gemm_configs_.snapshot();
  FftPlans::Snapshot fft_plans = fft_plans_.snapshot();

#if GOOGLE_CUDA || TF_HIPBLASLT
  MatmulPlans::Snapshot matmul_plans = gpublas_lt_matmul_plans_.snapshot();
#endif

#if GOOGLE_CUDA || TENSORFLOW_USE_ROCM
  StreamExecutorNormRunners::Snapshot norm_runners =
      norm_runners_(executor)->snapshot();
  StreamExecutorFusedAttentionRunners::Snapshot fused_attention_runners =
      fused_attention_runners_(executor)->snapshot();
  StreamExecutorFusedAttentionBackwardRunners::Snapshot
      fused_attention_backward_runners =
          fused_attention_backward_runners_(executor)->snapshot();
#endif  // GOOGLE_CUDA || TENSORFLOW_USE_ROCM

  // Pass auxiliary data to the custom call handlers.
  runtime::CustomCall::UserData user_data(
      run_options, &executable, &debug_options_, &temp_buffer, &asm_text,
      &binary, &kernels, &gemm_configs, &conv_runners, &collectives_,
      &fft_plans, &send_recv_events, &gpu_lock,
#if GOOGLE_CUDA || TF_HIPBLASLT
      &matmul_plans,
#endif
#if GOOGLE_CUDA
      // Auxiliary data that is available only if compiled with CUDA support
      // only.
      &norm_runners, &fused_attention_runners,
      &fused_attention_backward_runners,
#endif  // GOOGLE_CUDA
#if GOOGLE_CUDA || TENSORFLOW_USE_ROCM
      &graph_instances, &execution_count,
#endif  // GOOGLE_CUDA || TENSORFLOW_USE_ROCM
      &concurrent_region_status,
      // Null pointer will be interpreted as an absence of async collectives
      // support and custom calls will safely return an error.
      async_collectives.async_comm_stream(kAsyncStreamCollective)
          ? &async_collectives
          : nullptr);

  // Initialize state required for running functions from registered modules.
  auto state_ref = modules_state_.InitializeUserData(user_data);
  if (!state_ref.ok())
    return InternalError("Failed to initialize runtime modules state: %s",
                         state_ref.status().message());

#if GOOGLE_CUDA || TENSORFLOW_USE_ROCM
  // Instantiate all CUDA graphs before executing the main function.
  if (debug_options_.xla_gpu_graph_num_runs_to_instantiate() < 0 &&
      !graph_instances_.InstantiatedAllGraphs(run_options, executable)) {
    if (auto instantiated = graph_instances_.InstantiateAllGraphs(
            run_options, executable, user_data, buffer_allocations,
            buffer_sizes_, allocation_indices_,
            debug_options_.xla_gpu_graph_eviction_timeout_seconds());
        !instantiated.ok()) {
      return InternalError("Failed to instantiate GPU graphs: %s",
                           instantiated.message());
    }
  }
#endif  // GOOGLE_CUDA || TENSORFLOW_USE_ROCM

  // Collect all emitted diagnostic messages.
  std::string diagnostic;
  runtime::DiagnosticEngine diagnostic_engine;
  AppendDiagnosticToString(diagnostic_engine, &diagnostic, true);

  // Prepare options for executing XLA Runtime program.
  runtime::Executable::ExecuteOpts opts;
  opts.async_task_runner = NoAsyncTaskRunner();
  opts.custom_call_data = &user_data;
  opts.diagnostic_engine = &diagnostic_engine;
  opts.custom_call_registry = &dynamic_custom_calls_;

  // Execute with the prepared call frame.
  executable.Execute(call_frame, opts);

  if (auto st = executable.ReturnResults(converter, &call_frame); !st.ok()) {
    return InternalError("Failed to execute XLA Runtime executable: %s%s%s.",
                         st.message(), diagnostic.empty() ? "" : ": ",
                         diagnostic);
  }

  return absl::OkStatus();
}

//===----------------------------------------------------------------------===//

const Executable& GpuRuntimeExecutable::executable() const {
  if (auto* jit = std::get_if<std::unique_ptr<JitExecutable>>(&executable_)) {
    return *(*jit)->DefaultExecutable();
  }
  return *std::get<std::unique_ptr<Executable>>(executable_);
}

absl::StatusOr<std::string_view> GpuRuntimeExecutable::GetObjFile() const {
  if (auto obj_file = executable().obj_file())
    return std::string_view(obj_file->getBuffer());

  return InternalError("gpu runtime executable didn't save the obj file");
}

absl::StatusOr<std::string_view> GpuRuntimeExecutable::GetMlirModule() const {
  const auto* jit = std::get_if<std::unique_ptr<JitExecutable>>(&executable_);
  if (!jit) return InternalError("MLIR module is not available");

  return (*jit)->mlir_module();
}

}  // namespace gpu
}  // namespace xla
