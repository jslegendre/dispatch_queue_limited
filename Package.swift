// swift-tools-version: 5.7
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
    name: "dispatch_queue_limited",
    products: [
        // Products define the executables and libraries a package produces, and make them visible to other packages.
        .library(
            name: "dispatch_queue_limited",
            targets: ["dispatch_queue_limited"]),
    ],
    targets: [
        .target(
            name: "dispatch_queue_limited",
            dependencies: [],
            path: "Sources",
            cSettings: [
                .define("OS_ATOMIC_CONFIG_MEMORY_ORDER_DEPENDENCY"),
                .unsafeFlags(["-momit-leaf-frame-pointer", "-fno-modules", "-fno-objc-arc"])
            ]),
        .testTarget(
            name: "dispatch_queue_limitedTests",
            dependencies: ["dispatch_queue_limited"]),
    ]
)
