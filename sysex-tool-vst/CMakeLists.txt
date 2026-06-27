cmake_minimum_required(VERSION 3.25)

project(imwrap_sysex_tool_vst VERSION 0.1.0 LANGUAGES CXX)

set(SMTG_CREATE_MODULE_INFO OFF)
set(SMTG_RUN_VST_VALIDATOR OFF)

function(imwrap_target_setup_vst3 target_name)
    target_link_libraries(${target_name} PRIVATE
        sdk
        imwrap_v6
    )

    target_include_directories(${target_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/source
    )

    target_compile_features(${target_name} PUBLIC cxx_std_17)
    imwrap_apply_common_target_settings(${target_name})
endfunction()

smtg_add_vst3plugin(imwrap_sysex_tool
    source/factory.cpp
    source/SysExToolController.cpp
    source/SysExToolController.h
    source/SysExToolEditor.cpp
    source/SysExToolEditor.h
    source/SysExToolIds.h
    source/SysExToolModel.cpp
    source/SysExToolModel.h
    source/SysExToolProcessor.cpp
    source/SysExToolProcessor.h
    source/SysExToolStateIo.cpp
    source/SysExToolStateIo.h
    source/WinMidiOut.cpp
    source/WinMidiOut.h
    source/version.h
)

set_target_properties(imwrap_sysex_tool PROPERTIES
    FOLDER "Tools/VST3"
    OUTPUT_NAME "imwrap_sysex_tool"
)

imwrap_target_setup_vst3(imwrap_sysex_tool)

if(WIN32)
    target_link_libraries(imwrap_sysex_tool PRIVATE winmm)

    add_custom_command(TARGET imwrap_sysex_tool POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E rm -f
            "$<TARGET_FILE_DIR:imwrap_sysex_tool>/imwrap_sysex_tool_editor.dll"
            "$<TARGET_FILE_DIR:imwrap_sysex_tool>/Qt6Core.dll"
            "$<TARGET_FILE_DIR:imwrap_sysex_tool>/Qt6Gui.dll"
            "$<TARGET_FILE_DIR:imwrap_sysex_tool>/Qt6Widgets.dll"
            "$<TARGET_FILE_DIR:imwrap_sysex_tool>/opengl32sw.dll"
            "$<TARGET_FILE_DIR:imwrap_sysex_tool>/D3Dcompiler_47.dll"
            "$<TARGET_FILE_DIR:imwrap_sysex_tool>/dxcompiler.dll"
            "$<TARGET_FILE_DIR:imwrap_sysex_tool>/dxil.dll"
        COMMAND "${CMAKE_COMMAND}" -E remove_directory
            "$<TARGET_FILE_DIR:imwrap_sysex_tool>/platforms"
        COMMAND "${CMAKE_COMMAND}" -E remove_directory
            "$<TARGET_FILE_DIR:imwrap_sysex_tool>/styles"
    )
endif()
