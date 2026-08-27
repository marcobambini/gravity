// swift-tools-version: 6.0
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription
import CompilerPluginSupport

let package = Package(
    name: "Gravity",
    platforms: [
        .macOS(.v12),
        .iOS(.v16),
        .tvOS(.v16),
        .visionOS(.v1)
    ],
    products: [
        .library(
            name: "Gravity",
            targets: ["Gravity"]
        )
    ],
    dependencies: [
        .package(url: "https://github.com/swiftlang/swift-syntax", from: "602.0.0")
    ],
    targets: [
        .executableTarget(
            name: "GravitySwiftExample",
            dependencies: ["Gravity"],
            path: "examples/GravitySwiftExample",
            resources: [
                .copy("main.gravity"),
                .copy("extern.gravity")
            ]
        ),
        
        .target(
            name: "Gravity",
            dependencies: [
                "CGravity",
                "GravitySwiftMacros"
            ],
            path: "binding/GravitySwift"
        ),
        .target(
            name: "CGravity",
            path: "src",
            exclude: ["cli"],
            publicHeadersPath: "."
        ),
        .macro(
            name: "GravitySwiftMacros",
            dependencies: [
                .product(name: "SwiftSyntaxMacros", package: "swift-syntax"),
                .product(name: "SwiftCompilerPlugin", package: "swift-syntax")
            ],
            path: "binding/GravitySwiftMacros"
        ),
        .testTarget(
            name: "GravityTests",
            dependencies: ["Gravity"],
            path: "Tests/GravityTests"
        )
    ],
    cLanguageStandard: .gnu99,
    cxxLanguageStandard: .gnucxx11
)
