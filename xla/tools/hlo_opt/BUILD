load("//xla:glob_lit_test.bzl", "glob_lit_tests")
load(
    "//xla/stream_executor:build_defs.bzl",
    "if_gpu_is_configured",
)
load(
    "@local_config_rocm//rocm:build_defs.bzl",
    "if_rocm_is_configured",
)
load("@tsl//tsl:tsl.default.bzl", "filegroup")
load(
    "@tsl//tsl/platform:build_config_root.bzl",
    "tf_cuda_tests_tags",
)

# hlo-opt tool.
load(
    "@tsl//tsl/platform/default:cuda_build_defs.bzl",
    "if_cuda_is_configured",
)

package(
    # copybara:uncomment default_applicable_licenses = ["//tensorflow:license"],
    default_visibility = ["//xla:internal"],
    licenses = ["notice"],
)

# Includes a macro to register a provider.
cc_library(
    name = "opt_lib",
    srcs = ["opt_lib.cc"],
    hdrs = ["opt_lib.h"],
    deps = [
        "//xla:debug_options_flags",
        "//xla:statusor",
        "//xla:types",
        "//xla/hlo/ir:hlo",
        "//xla/service:compiler",
        "//xla/service:executable",
        "//xla/service:hlo_graph_dumper",
        "//xla/service:platform_util",
        "//xla/stream_executor",
        "//xla/stream_executor:platform",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/strings",
        "@tsl//tsl/platform:statusor",
    ],
)

cc_library(
    name = "gpu_opt",
    testonly = True,
    srcs = if_gpu_is_configured(["gpu_opt.cc"]),
    deps = [
        ":opt_lib",
        "//xla:debug_options_flags",
        "//xla:statusor",
        "//xla:types",
        "//xla/service:compiler",
        "//xla/service:dump",
        "//xla/service:executable",
        "//xla/service:hlo_graph_dumper",
        "//xla/service:platform_util",
        "//xla/service/gpu:executable_proto_cc",
        "//xla/stream_executor/platform",
        "@com_google_absl//absl/container:flat_hash_map",
        "@tsl//tsl/platform:statusor",
    ] + if_gpu_is_configured([
        "//xla/service:gpu_plugin",
        "//xla/service/gpu:gpu_executable",
    ]) + if_cuda_is_configured([
        "//xla/stream_executor:cuda_platform",
    ]) + if_rocm_is_configured([
        "//xla/stream_executor:rocm_platform",
    ]),
    alwayslink = True,  # Initializer needs to run.
)

cc_library(
    name = "cpu_opt",
    testonly = True,
    srcs = ["cpu_opt.cc"],
    deps = [
        ":opt_lib",
        "//xla/service:cpu_plugin",
        "//xla/service:hlo_graph_dumper",
        "//xla/stream_executor/host:host_platform",
        "//xla/stream_executor/platform",
    ],
    alwayslink = True,  # Initializer needs to run.
)

cc_library(
    name = "opt_main",
    testonly = True,
    srcs = ["opt_main.cc"],
    deps = [
        "cpu_opt",
        ":opt_lib",
        "//xla:debug_options_flags",
        "//xla:status",
        "//xla:statusor",
        "//xla/hlo/ir:hlo",
        "//xla/service:hlo_runner",
        "//xla/service:platform_util",
        "//xla/tools:hlo_module_loader",
        "//xla/tools:run_hlo_module_lib",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/strings",
        "@tsl//tsl/platform:env",
        "@tsl//tsl/platform:errors",
        "@tsl//tsl/platform:logging",
        "@tsl//tsl/platform:path",
        "@tsl//tsl/platform:platform_port",
        "@tsl//tsl/platform:status",
        "@tsl//tsl/platform:test",
        "@tsl//tsl/util:command_line_flags",
    ] + if_gpu_is_configured([
        ":gpu_opt",
    ]) + if_cuda_is_configured([
        "//xla/stream_executor:cuda_platform",
    ]) + if_rocm_is_configured([
        "//xla/stream_executor:rocm_platform",
    ]),
)

glob_lit_tests(
    name = "hlo_opt_tests",
    data = [":test_utilities"],
    default_tags = tf_cuda_tests_tags() + [
    ],
    driver = "//xla:run_lit.sh",
    features = if_cuda_is_configured([
        "--param=GPU=a100",
    ]) + if_rocm_is_configured([
        "--param=GPU=mi200",
    ]),
    tags_override = {
        "gpu_hlo_ptx.hlo": ["no_rocm"],
    },
    test_file_exts = ["hlo"],
)

# Bundle together all of the test utilities that are used by tests.
filegroup(
    name = "test_utilities",
    testonly = True,
    data = [
        "gpu_specs/a100.txtpb",
        "gpu_specs/mi200.txtpb",
        "//xla/tools:hlo-opt",
        "@llvm-project//llvm:FileCheck",
        "@llvm-project//mlir:run_lit.sh",
    ],
)

exports_files(glob([
    "gpu_specs/*.txtpb",
]))
