load("//xla:xla.bzl", "xla_cc_test")

package(
    # copybara:uncomment default_applicable_licenses = ["//tensorflow:license"],
    licenses = ["notice"],
)

cc_library(
    name = "sharding_conversions",
    srcs = ["sharding_conversions.cc"],
    hdrs = ["sharding_conversions.h"],
    visibility = ["//xla/python/ifrt:friends"],
    deps = [
        "//xla:shape_util",
        "//xla:statusor",
        "//xla:xla_data_proto_cc",
        "//xla/hlo/ir:hlo",
        "//xla/python/ifrt/ir",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@llvm-project//llvm:Support",
    ],
)

xla_cc_test(
    name = "sharding_conversions_test",
    srcs = ["sharding_conversions_test.cc"],
    deps = [
        ":sharding_conversions",
        "//xla:shape_util",
        "//xla:statusor",
        "//xla:xla_data_proto_cc",
        "//xla/hlo/ir:hlo",
        "//xla/python/ifrt",
        "//xla/python/ifrt:sharding_test_util",
        "//xla/python/ifrt/ir",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
        "@tsl//tsl/platform:status_matchers",
        "@tsl//tsl/platform:statusor",
    ],
)
