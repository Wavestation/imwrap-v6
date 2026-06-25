// swift-tools-version: 5.10

import PackageDescription

let package = Package(
    name: "imwrap-v6",
    platforms: [
        .macOS(.v13)
    ],
    products: [
        .library(name: "CIMWrapShim", targets: ["CIMWrapShim"]),
        .executable(name: "IMWrapAuthoringApp", targets: ["IMWrapAuthoringApp"]),
        .executable(name: "IMWrapSysExTool", targets: ["IMWrapSysExTool"]),
        .executable(name: "IMWrapPackerTool", targets: ["IMWrapPackerTool"])
    ],
    targets: [
        .target(
            name: "IMWrapCpp",
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
                "src/IMWrapEngine.cpp",
                "src/IMWrapSequence.cpp",
                "src/IMWrapSysex.cpp",
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
            name: "CIMWrapShim",
            dependencies: ["IMWrapCpp"],
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
            name: "IMWrapCoreSwift",
            path: "swift-app/core"
        ),
        .executableTarget(
            name: "IMWrapAuthoringApp",
            dependencies: ["CIMWrapShim", "IMWrapCoreSwift"],
            path: "swift-app/player"
        ),
        .executableTarget(
            name: "IMWrapSysExTool",
            path: "swift-app/sysex"
        ),
        .executableTarget(
            name: "IMWrapPackerTool",
            dependencies: ["IMWrapCoreSwift"],
            path: "swift-app/packer"
        )
    ],
    cxxLanguageStandard: .cxx17
)
