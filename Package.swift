// swift-tools-version: 6.0
import PackageDescription

// Stockfish 19 (tag sf_19) as built into ChessWOW for iOS and macOS.
// Flags mirror the upstream Makefile (-O3 -funroll-loops -DNDEBUG -DIS_64BIT
// -fno-exceptions -std=c++17) plus -DNNUE_EMBEDDING_OFF: the network file is shipped
// next to the code instead of being embedded. SIMD macros come from `onyx_arch.h`.
let stockfishFlags: [String] = [
    "-O3", "-funroll-loops", "-DNDEBUG",
    "-DIS_64BIT",
    "-DNNUE_EMBEDDING_OFF", "-DUSE_PTHREADS",
    "-fno-exceptions",
    "-include", "onyx_arch.h",
    "-Xarch_x86_64", "-mavx2", "-Xarch_x86_64", "-msse4.1",
    "-Xarch_x86_64", "-mssse3", "-Xarch_x86_64", "-mpopcnt",
    "-Wno-everything",
]

/// Dot product only where every supported device has it (Apple Silicon Macs).
let macOSArmFlags: [String] = ["-Xarch_arm64", "-march=armv8.2-a+dotprod"]

let package = Package(
    name: "CStockfish",
    platforms: [.iOS(.v17), .macOS(.v14)],
    products: [
        .library(name: "CStockfish", targets: ["CStockfish"]),
    ],
    targets: [
        .target(
            name: "CStockfish",
            path: "src",
            exclude: ["incbin/UNLICENCE"],
            publicHeadersPath: "include",
            cxxSettings: [
                .headerSearchPath("."),
                .unsafeFlags(stockfishFlags),
                .unsafeFlags(macOSArmFlags, .when(platforms: [.macOS])),
            ],
            linkerSettings: [
                .linkedLibrary("c++"),
            ]
        ),
    ],
    cxxLanguageStandard: .cxx17
)
