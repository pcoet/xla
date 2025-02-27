load("@local_config_cuda//cuda:build_defs.bzl", "cuda_library")
load("//xla:xla.bzl", "xla_cc_test")
load("//xla/service/gpu:build_defs.bzl", "gpu_kernel_library")
load("//xla/stream_executor:build_defs.bzl", "if_gpu_is_configured")
load(
    "@local_config_rocm//rocm:build_defs.bzl",
    "if_rocm_is_configured",
    "rocm_library",
)
load(
    "@tsl//tsl/platform:build_config_root.bzl",
    "tf_gpu_tests_tags",
)
load("@tsl//tsl/platform/default:cuda_build_defs.bzl", "if_cuda_is_configured")

package(
    # copybara:uncomment default_applicable_licenses = ["//tensorflow:license"],
    default_visibility = [":friends"],
    licenses = ["notice"],
)

package_group(
    name = "friends",
    includes = [
        "//xla:friends",
    ],
)

gpu_kernel_library(
    name = "gpu_kernel_helper",
    hdrs = if_gpu_is_configured(["gpu_kernel_helper.h"]),
    local_defines = if_cuda_is_configured(["GOOGLE_CUDA=1"]) + if_rocm_is_configured([
        "TENSORFLOW_USE_ROCM=1",
    ]),
    deps = [
        "//xla/stream_executor/platform",
        "@tsl//tsl/lib/math:math_util",
    ] + if_cuda_is_configured([
        "@local_config_cuda//cuda:cuda_headers",
    ]) + if_rocm_is_configured([
        "@local_config_rocm//rocm:rocm_headers",
    ]),
)

cc_library(
    name = "cholesky",
    srcs = ["cholesky.cc"],
    hdrs = ["cholesky.h"],
    local_defines = if_cuda_is_configured(["GOOGLE_CUDA=1"]) + if_rocm_is_configured([
        "TENSORFLOW_USE_ROCM=1",
    ]),
    deps = [
        ":support",
        "//xla:xla_proto_cc",
        "//xla/runtime:custom_call",
        "//xla/runtime:custom_call_registry",
        "//xla/runtime:executable",
        "//xla/service:executable",
        "//xla/service/gpu:gpu_asm_opts_util",
        "//xla/service/gpu/runtime3:cholesky_thunk",
    ],
)

cc_library(
    name = "collectives",
    srcs = ["collectives.cc"],
    hdrs = ["collectives.h"],
    local_defines = if_cuda_is_configured(["GOOGLE_CUDA=1"]) + if_rocm_is_configured([
        "TENSORFLOW_USE_ROCM=1",
    ]),
    deps = [
        ":support",
        "//xla/runtime:custom_call",
        "//xla/runtime:custom_call_registry",
        "//xla/runtime:executable",
        "//xla/service:collective_ops_utils",
        "//xla/service:computation_placer_hdr",
        "//xla/service:executable",
        "//xla/service:global_device_id",
        "//xla/service/gpu:gpu_executable_run_options",
        "//xla/service/gpu:nccl_collective_thunks",
        "//xla/service/gpu:thunk",
        "//xla/stream_executor",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ] + if_gpu_is_configured([
        "//xla/service/gpu:mock_nccl_utils",
    ]),
)

cc_library(
    name = "conv",
    srcs = ["conv.cc"],
    hdrs = ["conv.h"],
    local_defines = if_cuda_is_configured([
        "GOOGLE_CUDA=1",
    ]),
    deps = [
        ":support",
        "//xla:status",
        "//xla:xla_proto_cc",
        "//xla/mlir/runtime/transforms:custom_call_encoding",
        "//xla/runtime:custom_call",
        "//xla/runtime:custom_call_registry",
        "//xla/runtime:executable",
        "//xla/service:executable",
        "//xla/service/gpu:autotuner_util",
        "//xla/service/gpu:gpu_asm_opts_util",
        "//xla/service/gpu:gpu_conv_runner",
        "//xla/service/gpu:non_atomically_upgradeable_rw_lock",
        "//xla/stream_executor:device_memory",
        "//xla/stream_executor:device_memory_allocator",
        "//xla/translate/mhlo_to_hlo:attribute_exporter",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/synchronization",
        "@llvm-project//llvm:Support",
    ] + if_cuda_is_configured([
        "//xla/service/gpu:conv_algorithm_picker",
    ]),
)

cc_library(
    name = "conv_reorder",
    srcs = ["conv_reorder.cc"],
    hdrs = ["conv_reorder.h"],
    deps = [
        ":support",
        "//xla:xla_proto_cc",
        "//xla/runtime:custom_call",
        "//xla/runtime:custom_call_registry",
        "//xla/runtime:executable",
        "//xla/service:executable",
    ],
)

cc_library(
    name = "norm",
    srcs = ["norm.cc"],
    hdrs = ["norm.h"],
    deps = [
        ":support",
        "//xla:status",
        "//xla:xla_proto_cc",
        "//xla/mlir/runtime/transforms:custom_call_encoding",
        "//xla/runtime:custom_call",
        "//xla/runtime:custom_call_registry",
        "//xla/runtime:executable",
        "//xla/service:executable",
        "//xla/service/gpu:gpu_asm_opts_util",
        "//xla/service/gpu:gpu_norm_runner",
        "//xla/stream_executor:device_memory",
        "//xla/stream_executor:device_memory_allocator",
        "//xla/translate/mhlo_to_hlo:attribute_exporter",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/synchronization",
        "@llvm-project//llvm:Support",
    ],
)

cc_library(
    name = "fused_attention",
    srcs = ["fused_attention.cc"],
    hdrs = ["fused_attention.h"],
    deps = [
        ":support",
        "//xla:status",
        "//xla:xla_proto_cc",
        "//xla/mlir/runtime/transforms:custom_call_encoding",
        "//xla/runtime:custom_call",
        "//xla/runtime:custom_call_registry",
        "//xla/runtime:executable",
        "//xla/service:executable",
        "//xla/service/gpu:gpu_asm_opts_util",
        "//xla/service/gpu:gpu_fused_mha_runner",
        "//xla/stream_executor:device_memory",
        "//xla/stream_executor:device_memory_allocator",
        "//xla/translate/mhlo_to_hlo:attribute_exporter",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/synchronization",
        "@llvm-project//llvm:Support",
    ],
)

cc_library(
    name = "cub_sort",
    srcs = ["cub_sort.cc"],
    hdrs = ["cub_sort.h"],
    local_defines = if_cuda_is_configured(["GOOGLE_CUDA=1"]) + if_rocm_is_configured([
        "TENSORFLOW_USE_ROCM=1",
    ]),
    deps = [
        ":support",
        "//xla/runtime:custom_call",
        "//xla/runtime:custom_call_registry",
        "//xla/runtime:executable",
        "//xla/runtime:memref_view",
        "//xla/service:executable",
        "//xla/stream_executor:device_memory",
        "@com_google_absl//absl/status",
    ] + if_gpu_is_configured([
        "//xla/service/gpu:cub_sort_thunk",
    ]),
)

cc_library(
    name = "custom_call",
    srcs = ["custom_call.cc"],
    hdrs = ["custom_call.h"],
    copts = [
        "-fexceptions",
        "-fno-strict-aliasing",
    ],
    features = ["-use_header_modules"],
    local_defines = if_cuda_is_configured(["GOOGLE_CUDA=1"]) + if_rocm_is_configured([
        "TENSORFLOW_USE_ROCM=1",
    ]),
    deps = [
        ":support",
        ":triangular_solve",
        "//xla/runtime:custom_call_registry",
        "//xla/runtime:executable",
        "//xla/service:custom_call_status_internal",
        "//xla/service:custom_call_target_registry",
        "//xla/service:executable",
        "//xla/service/gpu:cublas_cudnn",
        "//xla/stream_executor/gpu:gpu_stream_header",
    ],
)

cc_library(
    name = "custom_call_registry",
    srcs = ["custom_call_registry.cc"],
    hdrs = ["custom_call_registry.h"],
    deps = ["//xla/runtime:custom_call_registry"],
)

cc_library(
    name = "executable",
    srcs = ["executable.cc"],
    hdrs = ["executable.h"],
    local_defines = if_cuda_is_configured(["GOOGLE_CUDA=1"]),
    deps = [
        ":cholesky",
        ":collectives",
        ":concurrent_region",
        ":conv",
        ":conv_reorder",
        ":cub_sort",
        ":custom_call",
        ":custom_call_registry",
        ":fft",
        ":fused_attention",
        ":gemm",
        ":gpublas_lt_matmul",
        ":graph_launch",
        ":io_feed",
        ":kernel_launch",
        ":memcpy",
        ":memset",
        ":norm",
        ":send_recv",
        ":stream_synchronization",
        ":support",
        ":topk",
        ":tracing",
        "//xla:statusor",
        "//xla:xla_proto_cc",
        "//xla/mlir/runtime/transforms:compilation_pipeline_gpu",
        "//xla/runtime:executable",
        "//xla/runtime:jit_executable",
        "//xla/runtime:module_registry",
        "//xla/service:executable",
        "//xla/service:stream_pool",
        "//xla/service/gpu:buffer_allocations",
        "//xla/service/gpu:non_atomically_upgradeable_rw_lock",
        "//xla/service/gpu:thunk",
        "//xla/stream_executor",
        "@com_google_absl//absl/container:inlined_vector",
        "@com_google_absl//absl/status",
        "@tsl//tsl/protobuf:dnn_proto_cc",
    ],
)

cc_library(
    name = "fft",
    srcs = ["fft.cc"],
    hdrs = ["fft.h"],
    deps = [
        ":support",
        "//xla/mlir/runtime/transforms:custom_call_encoding",
        "//xla/runtime:custom_call",
        "//xla/runtime:custom_call_registry",
        "//xla/runtime:executable",
        "//xla/runtime:state",
        "//xla/service/gpu/runtime3:fft_thunk",
        "//xla/stream_executor:fft",
    ],
)

cc_library(
    name = "topk_kernel",
    srcs = if_gpu_is_configured(["topk_kernel.cc"]),
    hdrs = if_gpu_is_configured(["topk_kernel.h"]),
    compatible_with = [],
    local_defines = if_cuda_is_configured(["GOOGLE_CUDA=1"]) + if_rocm_is_configured([
        "TENSORFLOW_USE_ROCM=1",
    ]),
    deps = [
        ":gpu_kernel_helper",
        ":support",
        "//xla:shape_util",
        "//xla:types",
        "//xla:util",
        "//xla:xla_data_proto_cc",
        "//xla/runtime:memref_view",
        "//xla/stream_executor",  # build_cleaner: keep
        "//xla/stream_executor:platform",
        "//xla/stream_executor/gpu:gpu_stream_header",
        "//xla/stream_executor/gpu:gpu_types_header",
        "@com_google_absl//absl/numeric:bits",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@eigen_archive//:eigen3",
        "@tsl//tsl/platform:errors",
        "@tsl//tsl/platform:statusor",
    ] + if_cuda_is_configured([
        ":topk_kernel_cuda",
    ]) + if_rocm_is_configured([
        ":topk_kernel_rocm",
    ]),
)

cuda_library(
    name = "topk_kernel_cuda",
    srcs = if_cuda_is_configured(
        [
            "topk_kernel_bfloat16.cu.cc",
            "topk_kernel_float.cu.cc",
            "topk_kernel.cu.h",
        ],
    ),
    hdrs = if_cuda_is_configured(["topk_kernel_common.h"]),
    compatible_with = [],
    local_defines = if_cuda_is_configured(["GOOGLE_CUDA=1"]),
    deps = [
        ":gpu_kernel_helper",
        "@eigen_archive//:eigen3",
    ],
)

rocm_library(
    name = "topk_kernel_rocm",
    srcs = if_rocm_is_configured(
        [
            "topk_kernel_bfloat16.cu.cc",
            "topk_kernel_float.cu.cc",
            "topk_kernel.cu.h",
        ],
    ),
    hdrs = if_rocm_is_configured(["topk_kernel_common.h"]),
    compatible_with = [],
    local_defines = if_rocm_is_configured(["TENSORFLOW_USE_ROCM=1"]),
    deps = [
        ":gpu_kernel_helper",
        "@eigen_archive//:eigen3",
    ],
)

xla_cc_test(
    name = "topk_kernel_test",
    srcs = if_gpu_is_configured(["topk_kernel_test.cc"]),
    tags = tf_gpu_tests_tags(),
    deps = [
        ":gpu_kernel_helper",
        ":topk_kernel",
        "//xla:xla_data_proto_cc",
        "//xla/stream_executor",  # build_cleaner: keep
        "//xla/stream_executor:multi_platform_manager",
        "//xla/stream_executor/gpu:gpu_init",
        "//xla/stream_executor/gpu:gpu_stream_header",
        "//xla/stream_executor/gpu:gpu_timer_header",
        "//xla/stream_executor/gpu:gpu_types_header",
        "//xla/stream_executor/host:host_platform",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@eigen_archive//:eigen3",
        "@tsl//tsl/platform:test",
        "@tsl//tsl/platform:test_benchmark",
        "@tsl//tsl/platform:test_main",
    ],
)

xla_cc_test(
    name = "topk_test",
    srcs = ["topk_test.cc"],
    local_defines = if_cuda_is_configured(["GOOGLE_CUDA=1"]) + if_rocm_is_configured([
        "TENSORFLOW_USE_ROCM=1",
    ]),
    tags = tf_gpu_tests_tags(),
    deps = [
        ":topk",
        "//xla:error_spec",
        "//xla:shape_util",
        "//xla:status",
        "//xla:statusor",
        "//xla:types",
        "//xla/hlo/ir:hlo",
        "//xla/service:gpu_plugin",
        "//xla/service:hlo_pass",
        "//xla/service:platform_util",
        "//xla/service:topk_rewriter",
        "//xla/service/gpu:topk_specializer",
        "//xla/tests:hlo_test_base",
        "//xla/tests:verified_hlo_module",
        "//xla/tests:xla_internal_test_main",  # fixdeps: keep
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/strings",
        "@tsl//tsl/platform:statusor",
        "@tsl//tsl/platform:test_main",
    ],
)

cc_library(
    name = "topk",
    srcs = if_gpu_is_configured(["topk.cc"]),
    hdrs = ["topk.h"],
    local_defines = if_cuda_is_configured(["GOOGLE_CUDA=1"]) + if_rocm_is_configured([
        "TENSORFLOW_USE_ROCM=1",
    ]),
    deps = if_gpu_is_configured([":topk_kernel"]) + [
        ":support",
        "//xla:executable_run_options",
        "//xla:shape_util",
        "//xla:status",
        "//xla:statusor",
        "//xla:types",
        "//xla:xla_data_proto_cc",
        "//xla:xla_proto_cc",
        "//xla/hlo/ir:hlo",
        "//xla/mlir/runtime/transforms:custom_call_encoding",
        "//xla/runtime:custom_call",
        "//xla/runtime:custom_call_registry",
        "//xla/runtime:executable",
        "//xla/runtime:state",
        "//xla/service:executable",
        "//xla/service:hlo_pass",
        "//xla/service:tuple_util",
        "//xla/stream_executor/gpu:gpu_stream_header",
        "//xla/stream_executor/gpu:gpu_types_header",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@tsl//tsl/platform:statusor",
    ],
)

cc_library(
    name = "gemm",
    srcs = ["gemm.cc"],
    hdrs = ["gemm.h"],
    local_defines = if_cuda_is_configured(["GOOGLE_CUDA=1"]) + if_rocm_is_configured([
        "TENSORFLOW_USE_ROCM=1",
    ]),
    deps = [
        ":support",
        "//xla:status",
        "//xla:xla_proto_cc",
        "//xla/runtime:custom_call",
        "//xla/runtime:custom_call_registry",
        "//xla/runtime:executable",
        "//xla/runtime:state",
        "//xla/service:executable",
        "//xla/service:hlo_module_config",
        "//xla/service/gpu:gpu_asm_opts_util",
        "//xla/service/gpu:matmul_utils",
        "//xla/service/gpu:non_atomically_upgradeable_rw_lock",
        "//xla/stream_executor:blas",
        "//xla/stream_executor:device_memory",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/status",
        "@tsl//tsl/platform:errors",
    ] + if_gpu_is_configured([
        "//xla/service/gpu:gemm_algorithm_picker",
        "//xla/stream_executor/gpu:redzone_allocator",
    ]),
)

cc_library(
    name = "graph_launch",
    srcs = ["graph_launch.cc"],
    hdrs = ["graph_launch.h"],
    local_defines = if_cuda_is_configured(["GOOGLE_CUDA=1"]) + if_rocm_is_configured([
        "TENSORFLOW_USE_ROCM=1",
    ]),
    deps = [
        ":concurrent_region",
        ":conv",
        ":gemm",
        ":kernel_launch",
        ":support",
        "//xla:statusor",
        "//xla/runtime:custom_call",
        "//xla/runtime:custom_call_registry",
        "//xla/runtime:executable",
        "//xla/service:executable",
        "//xla/service/gpu:buffer_allocations",
        "//xla/service/gpu:non_atomically_upgradeable_rw_lock",
        "//xla/stream_executor",
        "//xla/stream_executor/gpu:gpu_graph",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/types:span",
        "@tsl//tsl/profiler/lib:profiler_lock",
        "@tsl//tsl/profiler/lib:traceme",
        "@tsl//tsl/profiler/lib:traceme_encode",
    ],
)

cc_library(
    name = "concurrent_region",
    srcs = ["concurrent_region.cc"],
    hdrs = ["concurrent_region.h"],
    deps = [
        ":support",
        "//xla/runtime:custom_call",
        "//xla/runtime:custom_call_registry",
        "//xla/runtime:executable",
        "//xla/service:executable",
        "//xla/service:stream_pool",
        "//xla/stream_executor",
        "@tsl//tsl/platform:statusor",
    ],
)

cc_library(
    name = "stream_synchronization",
    srcs = ["stream_synchronization.cc"],
    hdrs = ["stream_synchronization.h"],
    deps = [
        ":concurrent_region",
        ":support",
        "//xla/runtime:custom_call_registry",
        "//xla/runtime:executable",
    ],
)

cc_library(
    name = "io_feed",
    srcs = ["io_feed.cc"],
    hdrs = ["io_feed.h"],
    deps = [
        ":support",
        "//xla/runtime:custom_call",
        "//xla/runtime:custom_call_registry",
        "//xla/runtime:executable",
        "//xla/service:executable",
        "//xla/service/gpu:io_feed_manager",
    ],
)

cc_library(
    name = "kernel_launch",
    srcs = ["kernel_launch.cc"],
    hdrs = ["kernel_launch.h"],
    local_defines = if_cuda_is_configured(["GOOGLE_CUDA=1"]) + if_rocm_is_configured([
        "TENSORFLOW_USE_ROCM=1",
    ]),
    deps = [
        ":concurrent_region",
        ":support",
        "//xla:statusor",
        "//xla/hlo/ir:hlo",
        "//xla/runtime:custom_call",
        "//xla/runtime:custom_call_registry",
        "//xla/runtime:executable",
        "//xla/runtime:memref_view",
        "//xla/runtime:state",
        "//xla/service:executable",
        "//xla/service:hlo_proto_cc",
        "//xla/service/gpu:launch_dimensions",
        "//xla/service/gpu:stream_executor_util",
        "//xla/service/gpu/kernels:custom_fusion",
        "//xla/service/gpu/kernels:custom_kernel",
        "//xla/stream_executor",
        "//xla/stream_executor/gpu:gpu_graph",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:inlined_vector",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
        "@tsl//tsl/platform:errors",
        "@tsl//tsl/platform:logging",
    ],
)

cc_library(
    name = "gpublas_lt_matmul",
    srcs = ["gpublas_lt_matmul.cc"],
    hdrs = ["gpublas_lt_matmul.h"],
    local_defines = if_cuda_is_configured(["GOOGLE_CUDA=1"]) + if_rocm_is_configured([
        "TENSORFLOW_USE_ROCM=1",
    ]),
    deps = [
        ":support",
        "//xla:xla_proto_cc",
        "//xla/mlir/runtime/transforms:custom_call_encoding",
        "//xla/runtime:custom_call",
        "//xla/runtime:custom_call_registry",
        "//xla/runtime:executable",
        "//xla/runtime:logical_result",
        "//xla/runtime:state",
        "//xla/service:executable",
        "//xla/service/gpu:matmul_utils",
        "//xla/stream_executor",
        "@tsl//tsl/platform:status",
    ] + if_rocm_is_configured([
        "@local_config_rocm//rocm:rocm_headers",
    ]),
)

cc_library(
    name = "memcpy",
    srcs = ["memcpy.cc"],
    hdrs = ["memcpy.h"],
    deps = [
        ":concurrent_region",
        ":support",
        "//xla/runtime:custom_call",
        "//xla/runtime:custom_call_registry",
        "//xla/runtime:executable",
        "//xla/service:executable",
    ],
)

cc_library(
    name = "memset",
    srcs = ["memset.cc"],
    hdrs = ["memset.h"],
    deps = [
        ":support",
        "//xla/runtime:custom_call",
        "//xla/runtime:custom_call_registry",
        "//xla/runtime:executable",
        "//xla/service:executable",
        "@com_google_absl//absl/base",
    ],
)

cc_library(
    name = "support",
    srcs = ["support.cc"],
    hdrs = ["support.h"],
    local_defines = if_cuda_is_configured(["GOOGLE_CUDA=1"]) + if_rocm_is_configured([
        "TENSORFLOW_USE_ROCM=1",
    ]),
    deps = [
        "//xla:shape_util",
        "//xla/mlir/runtime/transforms:custom_call_encoding",
        "//xla/runtime:custom_call",
        "//xla/service/gpu:matmul_utils",
        "//xla/stream_executor:blas",
        "//xla/stream_executor:device_memory",
        "@com_google_absl//absl/strings",
        "@llvm-project//llvm:Support",
        "@tsl//tsl/profiler/lib:scoped_annotation_stack",
    ],
)

cc_library(
    name = "send_recv",
    srcs = ["send_recv.cc"],
    hdrs = ["send_recv.h"],
    deps = [
        ":support",
        "//xla/mlir/runtime/transforms:custom_call_encoding",
        "//xla/mlir_hlo",
        "//xla/runtime:custom_call",
        "//xla/runtime:custom_call_registry",
        "//xla/runtime:executable",
        "//xla/service:executable",
        "//xla/stream_executor",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
        "@tsl//tsl/concurrency:async_value",
        "@tsl//tsl/profiler/lib:traceme",
        "@tsl//tsl/profiler/lib:traceme_encode",
    ],
)

cc_library(
    name = "tracing",
    srcs = [
        "annotation.cc",
        "tracing.cc",
    ],
    hdrs = [
        "annotation.h",
        "tracing.h",
    ],
    local_defines = if_cuda_is_configured(["GOOGLE_CUDA=1"]),
    deps = [
        ":support",
        "//xla/hlo/ir:hlo",
        "//xla/runtime:custom_call_registry",
        "//xla/runtime:executable",
        "//xla/runtime:tracing",
        "//xla/runtime:type_id",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@tsl//tsl/profiler/lib:nvtx_utils",
        "@tsl//tsl/profiler/lib:scoped_annotation_stack",
    ],
)

cc_library(
    name = "triangular_solve",
    srcs = ["triangular_solve.cc"],
    hdrs = ["triangular_solve.h"],
    local_defines = if_cuda_is_configured(["GOOGLE_CUDA=1"]) + if_rocm_is_configured(["TENSORFLOW_USE_ROCM"]),
    deps = [
        ":support",
        "//xla:xla_proto_cc",
        "//xla/runtime:custom_call",
        "//xla/service:executable",
        "//xla/service/gpu:gpu_asm_opts_util",
        "//xla/service/gpu/runtime3:triangular_solve_thunk",
        "@tsl//tsl/platform:human_readable_json",
    ],
)
