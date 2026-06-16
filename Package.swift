// swift-tools-version: 5.10

import PackageDescription

let package = Package(
    name: "imuse-v6",
    platforms: [
        .macOS(.v13)
    ],
    products: [
        .library(name: "CImuseShim", targets: ["CImuseShim"]),
        .executable(name: "ImuseAuthoringApp", targets: ["ImuseAuthoringApp"]),
        .executable(name: "ImuseSysExTool", targets: ["ImuseSysExTool"]),
        .executable(name: "ImusePackerTool", targets: ["ImusePackerTool"])
    ],
    targets: [
        .target(
            name: "ImuseCpp",
            path: ".",
            exclude: [
                "baka",
                "build",
                "docs",
                "swift-app",
                "swift-shim",
                "tools",
                "CMakeLists.txt",
                "README.md"
            ],
            sources: [
                "src/Instrument.cpp",
                "src/ImuseEngine.cpp",
                "src/ImuseSequence.cpp",
                "src/ImuseSysex.cpp",
                "src/ImsWriter.cpp",
                "src/ResourceBank.cpp",
                "src/SinkMidiChannel.cpp",
                "src/SmfSequence.cpp"
            ],
            publicHeadersPath: "include",
            cxxSettings: [
                .headerSearchPath("include")
            ]
        ),
        .target(
            name: "CImuseShim",
            dependencies: ["ImuseCpp"],
            path: "swift-shim",
            publicHeadersPath: "include",
            cxxSettings: [
                .headerSearchPath("../include"),
                .headerSearchPath("../third_party/fluidsynth-build/include"),
                .headerSearchPath("../third_party/fluidsynth-master/include"),
                .headerSearchPath("../third_party/munt/mt32emu/src"),
                .headerSearchPath("../third_party/mt32emu-build/include"),
                .headerSearchPath("../third_party/mt32emu-build/include/mt32emu"),
                .headerSearchPath("../third_party/libadlmidi/include")
            ],
            linkerSettings: [
                .unsafeFlags([
                    "-L", "third_party/fluidsynth-build/src",
                    "-lfluidsynth",
                    "-L", "third_party/mt32emu-build",
                    "-lmt32emu",
                    "-L", "third_party/adlmidi-build",
                    "-lADLMIDI",
                    "-framework", "CoreAudio",
                    "-framework", "AudioUnit",
                    "-lm"
                ])
            ]
        ),
        .target(
            name: "ImuseCoreSwift",
            path: "swift-app/core"
        ),
        .executableTarget(
            name: "ImuseAuthoringApp",
            dependencies: ["CImuseShim", "ImuseCoreSwift"],
            path: "swift-app/player"
        ),
        .executableTarget(
            name: "ImuseSysExTool",
            path: "swift-app/sysex"
        ),
        .executableTarget(
            name: "ImusePackerTool",
            dependencies: ["ImuseCoreSwift"],
            path: "swift-app/packer"
        )
    ],
    cxxLanguageStandard: .cxx17
)
