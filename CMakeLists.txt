cmake_minimum_required(VERSION 3.22)

# 定义项目名称与版本
project(devpiano VERSION 0.4.0)
# ========================================================
# 1. 引入 JUCE 框架
# ========================================================
# 告诉 CMake 将 JUCE 目录作为子项目添加进构建树
add_subdirectory(submodules/JUCE)

# JIVE: 声明式 UI 框架（ValueTree 布局 + JSON 样式表 + Flex/Grid）
add_subdirectory(submodules/JIVE)

# melatonin_inspector: 运行时 Component 检查器（DEBUG 构建可用）
add_subdirectory(submodules/melatonin_inspector)

# ========================================================
# 2. 创建 GUI 应用程序
# ========================================================
juce_add_gui_app(devpiano
    COMPANY_NAME "0xnayuta/devpiano"      # 请替换为你的组织名
    BUNDLE_IDENTIFIER com.yourcompany.devpiano
    PRODUCT_NAME "DevPiano"
)

# ========================================================
# 3. 设置 C++ 标准与编译选项
# ========================================================
target_compile_features(devpiano PRIVATE cxx_std_20)
set_target_properties(devpiano PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(devpiano PRIVATE
        -Wall
        -Wextra
    )
endif()


if (MSVC)
    target_compile_options(devpiano PRIVATE
        /utf-8
        /permissive-
        /Zc:__cplusplus
        /W4
    )
endif()

# ========================================================
# 4. 添加源代码
# ========================================================
target_sources(devpiano PRIVATE
    source/Main.cpp
    source/MainComponent.cpp
    source/MainComponent.h
    source/MainComponentJiveAccessors.cpp
    source/Audio/AudioEngine.cpp
    source/Audio/AudioEngine.h
    source/Audio/SineSynthVoice.h
    source/Audio/PianoSynthVoice.h
    source/Audio/AudioDeviceDiagnostics.h
    source/Input/KeyboardMidiMapper.cpp
    source/Input/KeyboardMidiMapper.h
    source/Core/KeyMapTypes.h
    source/Core/MidiTypes.h
    source/Core/AppState.h
    source/Midi/MidiChannelMapper.cpp
    source/Midi/MidiChannelMapper.h
    source/Midi/ChannelMatrix.h
    source/Plugin/PluginHost.cpp
    source/Plugin/PluginHost.h
    source/Plugin/PluginFlowSupport.cpp
    source/Plugin/PluginFlowSupport.h
    source/Plugin/PluginOperationController.cpp
    source/Plugin/PluginOperationController.h
    source/Export/ExportFlowSupport.cpp
    source/Export/ExportFlowSupport.h
    source/Export/WavExportTask.cpp
    source/Export/WavExportTask.h
    source/Export/WavExportOptions.h
    source/Recording/RecordingEngine.cpp
    source/Recording/RecordingEngine.h
    source/Recording/RecordingFlowSupport.cpp
    source/Recording/RecordingFlowSupport.h
    source/Recording/RecordingSessionController.cpp
    source/Recording/RecordingSessionController.h
    source/Recording/MidiFileExporter.cpp
    source/Recording/MidiFileExporter.h
    source/Recording/MidiFileImporter.cpp
    source/Recording/MidiFileImporter.h
    source/Recording/PerformanceFile.cpp
    source/Recording/PerformanceFile.h
    source/Recording/RenderPipeline.cpp
    source/Recording/RenderPipeline.h
    source/Recording/WavFileExporter.cpp
    source/Recording/WavFileExporter.h
    source/Recording/PluginOfflineRenderer.cpp
    source/Recording/PluginOfflineRenderer.h
    source/UI/PluginEditorWindow.cpp
    source/UI/PluginEditorWindow.h
    source/UI/PluginPanelStateBuilder.cpp
    source/UI/PluginPanelStateBuilder.h
    source/UI/DevPianoLookAndFeel.cpp
    source/UI/DevPianoLookAndFeel.h
    source/Settings/SettingsModel.h
    source/Settings/AppStateBuilder.cpp
    source/Settings/AppStateBuilder.h
    source/UI/CustomKeyboard.cpp
    source/UI/CustomKeyboard.h
    source/UI/ComboSelection.h
    source/UI/native/AdsrCurveComponent.cpp
    source/UI/native/AdsrCurveComponent.h
    source/UI/native/StatusBarMidiDot.h
    source/UI/native/KeyboardViewport.h
    source/UI/KeyboardTypes.h
    source/UI/PluginTypes.h
    source/UI/RecordingTypes.h
    source/Settings/SettingsStore.cpp
    source/UI/KeyBindingEditDialog.cpp
    source/UI/KeyBindingEditDialog.h
    source/UI/PerformanceMetadataDialog.cpp
    source/UI/PerformanceMetadataDialog.h
    source/UI/PresetDialogs.cpp
    source/UI/PresetDialogs.h
    source/Settings/SettingsStore.h
    source/Settings/SettingsSerialization.cpp
    source/Settings/SettingsSerialization.h
    source/Settings/SettingsComponent.h
    source/Settings/SettingsWindowManager.cpp
    source/Settings/SettingsWindowManager.h
    source/Layout/PerformancePreset.cpp
    source/Layout/PerformancePreset.h
    source/Layout/PresetFlowSupport.cpp
    source/Layout/PresetFlowSupport.h
    source/Diagnostics/Log.h
    source/Diagnostics/DevPianoLogger.h
    source/Diagnostics/DevPianoLogger.cpp
    source/Diagnostics/MidiTrace.h
    source/Diagnostics/MidiTrace.cpp
    source/Locale/LocaleManager.h
    source/Locale/zh_CN.loc.h
    source/UI/jive/DesignTokens.cpp
    source/UI/jive/DesignTokens.h
    source/UI/jive/StyleCatalog.cpp
    source/UI/jive/StyleCatalog.h
    source/UI/jive/LayoutModel.cpp
    source/UI/jive/LayoutModel.h
)
# 包含 source 目录作为头文件查找路径，这样代码里可以通过 #include "xxx.h" 直接引用
target_include_directories(devpiano PRIVATE source)

juce_generate_juce_header(devpiano)

# ========================================================
# 5. 链接 JUCE 模块
# ========================================================
target_link_libraries(devpiano PRIVATE
    juce::juce_audio_devices      # 处理 ASIO/WASAPI 音频设备
    juce::juce_audio_formats      # 处理音频文件
    juce::juce_audio_processors   # 核心：处理 VST/VST3 插件和 MIDI 缓冲
    juce::juce_audio_utils        # 提供 AudioDeviceManager 等实用工具
    juce::juce_core               # 核心类型、线程、文件操作
    juce::juce_data_structures    # ValueTree（用于按键映射的配置存储）
    juce::juce_events             # 事件循环与消息队列
    juce::juce_graphics           # 2D 绘图
    juce::juce_gui_basics         # 基础组件、窗口
    juce::juce_gui_extra          # 额外 UI 组件
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
    jive::jive_layouts            # JIVE: ValueTree 布局引擎
    jive::jive_style_sheets       # JIVE: JSON 样式表引擎
    melatonin_inspector           # 运行时 Component 检查器
)

# ========================================================
# 6. 配置预编译宏（开启插件宿主支持）
# ========================================================
target_compile_definitions(devpiano PRIVATE
    # 启用模态循环（JUCE 8 默认为 0；WAV 导出进度对话框需要该值为 1）
    JUCE_MODAL_LOOPS_PERMITTED=1

    # 禁用一些不需要的沉重组件以加快编译
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_DIRECTWRITE=0
    NOMINMAX=1
    WIN32_LEAN_AND_MEAN=1

    # 核心：允许我们的应用加载外部 VST3 插件（VST3 插件音源）
    # JIVE：同时使用 jive_layouts 和 jive_style_sheets 时推荐定义
    JIVE_GUI_ITEMS_HAVE_STYLE_SHEETS=1
    JUCE_PLUGINHOST_VST3=1

    # 如果你也想加载古老的 VST2 插件，可以解开下方的注释
    # 但前提是你必须持有 Steinberg 的旧版 VST2 SDK 并在 CMake 中配置路径
    # JUCE_PLUGINHOST_VST=1
)

if (MSVC)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT devpiano)
endif()

# ========================================================
# 7. 单元测试
# ========================================================
option(BUILD_TESTS "Build unit tests" OFF)

if (BUILD_TESTS)
    juce_add_console_app(devpiano_tests
        PRODUCT_NAME "DevPiano Tests"
    )

    target_compile_features(devpiano_tests PRIVATE cxx_std_20)
    set_target_properties(devpiano_tests PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(devpiano_tests PRIVATE
            -Wall
            -Wextra
        )
    endif()
    if (MSVC)
        target_compile_options(devpiano_tests PRIVATE
            /utf-8
            /permissive-
            /Zc:__cplusplus
            /W4
        )
    endif()
    # 生产代码（被测模块）：只包含无 GUI/设备依赖的纯逻辑层
    target_sources(devpiano_tests PRIVATE
        source/Core/KeyMapTypes.h
        source/Core/MidiTypes.h
        source/Core/AppState.h
        source/Input/KeyboardMidiMapper.cpp
        source/Input/KeyboardMidiMapper.h
        source/Diagnostics/Log.h
        source/Diagnostics/DevPianoLogger.h
        source/Diagnostics/DevPianoLogger.cpp
        source/Diagnostics/MidiTrace.cpp
        source/Diagnostics/MidiTrace.h
        source/Recording/MidiFileImporter.cpp
        source/Recording/MidiFileImporter.h
        source/Recording/MidiFileExporter.cpp
        source/Recording/MidiFileExporter.h
        source/Recording/WavFileExporter.cpp
        source/Recording/WavFileExporter.h
        source/Recording/PerformanceFile.cpp
        source/Recording/PerformanceFile.h
        source/Recording/RenderPipeline.cpp
        source/Recording/RenderPipeline.h
        source/Recording/RecordingEngine.cpp
        source/Recording/RecordingEngine.h
        source/Recording/RecordingFlowSupport.cpp
        source/Recording/RecordingFlowSupport.h

        source/Export/ExportFlowSupport.cpp
        source/Export/ExportFlowSupport.h
        source/Export/WavExportOptions.h

        source/Midi/MidiChannelMapper.cpp
        source/Midi/MidiChannelMapper.h
        source/Audio/AudioEngine.cpp
        source/Audio/AudioEngine.h
        source/Audio/SineSynthVoice.h
        source/Audio/PianoSynthVoice.h
        source/Plugin/PluginHost.cpp
        source/Plugin/PluginHost.h

        source/Layout/PerformancePreset.cpp
        source/Layout/PerformancePreset.h
        source/Settings/SettingsStore.cpp
        source/Settings/SettingsStore.h
        source/Settings/SettingsSerialization.cpp
        source/Settings/SettingsSerialization.h
        source/Settings/SettingsModel.h
        source/UI/KeyboardTypes.h
        source/UI/RecordingTypes.h
        source/UI/PluginPanelStateBuilder.cpp
        source/UI/PluginPanelStateBuilder.h
        source/UI/PluginTypes.h
        source/UI/CustomKeyboard.cpp
        source/UI/CustomKeyboard.h

        source/UI/jive/StyleCatalog.cpp
        source/UI/jive/StyleCatalog.h
        source/UI/jive/LayoutModel.cpp
        source/UI/jive/LayoutModel.h
        source/UI/jive/DesignTokens.cpp
        source/UI/jive/DesignTokens.h
        source/UI/DevPianoLookAndFeel.cpp
        source/UI/DevPianoLookAndFeel.h



        # 测试代码
        source/tests/TestRunner.cpp
        source/tests/KeyMapTypesTest.cpp
        source/tests/MidiFileImporterTest.cpp
        source/tests/RecordingEngineTest.cpp
        source/tests/KeyboardMidiMapperTest.cpp
        source/tests/AudioEngineTest.cpp
        source/tests/PluginHostTest.cpp
        source/tests/PerformanceFileTest.cpp
        source/tests/RenderPipelineTest.cpp
        source/tests/StyleCatalogTest.cpp
        source/tests/PathEditorReproTest.cpp
        source/tests/RecordingSessionControllerTest.cpp
        source/tests/MidiChannelMapperTest.cpp
        source/tests/PerformancePresetTest.cpp
        source/tests/SettingsStoreTest.cpp
        source/tests/ExportFlowTest.cpp
        source/tests/PluginHostXmlTest.cpp
        source/tests/KeyboardHitMappingTest.cpp
        source/tests/PianoSynthVoiceTest.cpp
    )
    target_link_libraries(devpiano_tests PRIVATE
        juce::juce_core
        juce::juce_events
        juce::juce_gui_basics
        juce::juce_audio_basics
        juce::juce_audio_formats
        juce::juce_audio_devices
        juce::juce_audio_processors
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
        jive::jive_core
        jive::jive_layouts
        jive::jive_style_sheets
    )

    target_include_directories(devpiano_tests PRIVATE source)
    juce_generate_juce_header(devpiano_tests)
    target_compile_definitions(devpiano_tests PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_UNIT_TESTS=1
        JIVE_GUI_ITEMS_HAVE_STYLE_SHEETS=1
    )

    enable_testing()

    add_test(NAME devpiano_tests
             COMMAND devpiano_tests
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# clang-tidy target (requires compile_commands.json)
find_program(CLANG_TIDY_EXE NAMES clang-tidy clang-tidy-21 clang-tidy-20 clang-tidy-19)
if(CLANG_TIDY_EXE)
    # GLOB 仅作 configure 变更触发源（CONFIGURE_DEPENDS：新增源文件自动重新 configure）
    file(GLOB_RECURSE ALL_SOURCE_FILES CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/source/*.cpp"
        "${CMAKE_SOURCE_DIR}/source/*.h"
    )
    # 去重：只分析 compile_commands.json 中实际编译的 source/ 源文件。
    # 同一文件因 devpiano/devpiano_tests 双目标产生多条记录时只保留一次；
    # 被 #include 的孤儿 .cpp（无独立编译记录）不重复分析。
    set(TIDY_FILES)
    if(EXISTS "${CMAKE_BINARY_DIR}/compile_commands.json")
        file(STRINGS "${CMAKE_BINARY_DIR}/compile_commands.json" _CC_LINES REGEX "\"file\"[ \t]*:")
        foreach(_CC_LINE IN LISTS _CC_LINES)
            if(_CC_LINE MATCHES "\"file\"[ \t]*:[ \t]*\"([^\"]+)\"")
                set(_CC_FILE "${CMAKE_MATCH_1}")
                if(_CC_FILE MATCHES "${CMAKE_SOURCE_DIR}/source/")
                    list(APPEND TIDY_FILES "${_CC_FILE}")
                endif()
            endif()
        endforeach()
        list(REMOVE_DUPLICATES TIDY_FILES)
    endif()
    if(NOT TIDY_FILES)
        # compile_commands 缺失时降级为 GLOB 全量
        set(TIDY_FILES ${ALL_SOURCE_FILES})
    endif()
    add_custom_target(clang-tidy
        COMMAND ${CLANG_TIDY_EXE}
            -p "${CMAKE_BINARY_DIR}"
            --quiet
            ${TIDY_FILES}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Running clang-tidy on source/ (${TIDY_FILES} files)"
        VERBATIM
    )
    message(STATUS "clang-tidy target: enabled (${CLANG_TIDY_EXE})")
else()
    message(STATUS "clang-tidy target: skipped (clang-tidy not found)")
endif()
