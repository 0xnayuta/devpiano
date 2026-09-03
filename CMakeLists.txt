cmake_minimum_required(VERSION 3.22)

# 强制全局所有子目录（JUCE, JIVE 等）默认采用 CMP0141 NEW (Embedded /Z7 调试符号)
set(CMAKE_POLICY_DEFAULT_CMP0141 NEW)
if (POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
endif()
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>" CACHE STRING "" FORCE)

# 定义项目名称与版本
project(devpiano VERSION 1.0.2)
if (MSVC)
    # 将默认 /Zi 替换为 /Z7 (Embedded 调试符号，支持 sccache 分布式缓存与并发无锁编译)
    string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
    string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")

    # 启用 /FS (Force Synchronous PDB Writes) 彻底杜绝 C1041 锁冲突
    add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:/FS>)

    # 链接器性能优化：
    # 1. 禁用 /INCREMENTAL（避免生成增量链接垫片与 ILINK 锁开销）
    # 2. Release LTCG 启用 8 线程并发代码生成 (/CGTHREADS:8)
    add_link_options("/INCREMENTAL:NO" "$<$<CONFIG:Release,RelWithDebInfo>:/CGTHREADS:8>")
endif()
# ========================================================
# 高速链接器检测与启用 (mold / lld 极大缩短链接时间)
# ========================================================
if (NOT MSVC AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    find_program(MOLD_LINKER_EXE mold)
    find_program(LLD_LINKER_EXE NAMES ld.lld lld)
    if (MOLD_LINKER_EXE)
        add_link_options("-fuse-ld=mold")
        message(STATUS "High-speed linker: mold enabled (${MOLD_LINKER_EXE})")
    elseif (LLD_LINKER_EXE)
        add_link_options("-fuse-ld=lld")
        message(STATUS "High-speed linker: lld enabled (${LLD_LINKER_EXE})")
    endif()
endif()

# ========================================================
# 编译耗时性能跟踪与火焰图剖析 (-ftime-trace)
# ========================================================
option(ENABLE_TIME_TRACE "Enable Clang -ftime-trace profiling" OFF)
if (ENABLE_TIME_TRACE)
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(-ftime-trace)
        message(STATUS "Build Profiling: Clang -ftime-trace enabled")
    else()
        message(WARNING "ENABLE_TIME_TRACE is only supported for Clang compilers")
    endif()
endif()


# ========================================================
# 1. 引入 JUCE 框架
# ========================================================
# 告诉 CMake 将 JUCE 目录作为子项目添加进构建树
add_subdirectory(submodules/JUCE)

# 清理 juce_recommended_config_flags 中可能残留的 /Zi 接口选项，确保 100% 使用 /Z7
if (MSVC AND TARGET juce_recommended_config_flags)
    get_target_property(_juce_opts juce_recommended_config_flags INTERFACE_COMPILE_OPTIONS)
    if (_juce_opts)
        string(REPLACE "/Zi" "/Z7" _juce_opts_patched "${_juce_opts}")
        set_target_properties(juce_recommended_config_flags PROPERTIES
            INTERFACE_COMPILE_OPTIONS "${_juce_opts_patched}"
        )
    endif()
endif()



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

if (MSVC)
    set_target_properties(devpiano PROPERTIES
        MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>"
    )
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(devpiano PRIVATE
        -Wall
        -Wextra
        # ADR-011 决策 2：PCH 与编译缓存（ccache/sccache）协同——Clang PCH
        # 默认内嵌创建时间戳，ccache 缓存重放的 PCH 文件时间戳与原始不同，
        # 会触发 "file modified since precompiled header was built" 使缓存
        # 失效。-fno-pch-timestamp 关闭时间戳校验（内容哈希仍生效），是
        # ccache 官方手册对 Clang PCH 场景的推荐配置（cc1 选项须经 -Xclang）。
        -Xclang
        -fno-pch-timestamp
    )
endif()

if (MSVC)
    target_compile_options(devpiano PRIVATE
        /utf-8
        /permissive-
        /Zc:__cplusplus
        /W4
        # JUCE 模块 .c 文件使 target 含 C 语言，CMake 为 C 生成 PCH 桩；
        # pch.h 为 STL-only（ADR-011），C 模式下为空翻译单元触发 C4206，无害；
        # JUCE 第三方 C 模块（如 SheenBidi SBBase.c）在 Release 优化下未引用内部静态函数触发 C4505，无害
        $<$<COMPILE_LANGUAGE:C>:/wd4206 /wd4505>
    )
endif()

# ========================================================
# 3.1 JIVE 核心最小依赖闭包（内化自 JIVE，依据 ADR-014）
# ========================================================
set(DEVPIANO_JIVE_CORE_SOURCES
    source/UI/jive/core/jive_AttributedStringVariantConverters.cpp
    source/UI/jive/core/jive_BackgroundCanvas.cpp
    source/UI/jive/core/jive_BlockContainer.cpp
    source/UI/jive/core/jive_BlockItem.cpp
    source/UI/jive/core/jive_BoxModel.cpp
    source/UI/jive/core/jive_Button.cpp
    source/UI/jive/core/jive_Canvas.cpp
    source/UI/jive/core/jive_Colours.cpp
    source/UI/jive/core/jive_ComboBox.cpp
    source/UI/jive/core/jive_CommonGuiItem.cpp
    source/UI/jive/core/jive_ComponentFactory.cpp
    source/UI/jive/core/jive_ComponentInteractionState.cpp
    source/UI/jive/core/jive_ContainerItem.cpp
    source/UI/jive/core/jive_ContainerItemChild.cpp
    source/UI/jive/core/jive_Display.cpp
    source/UI/jive/core/jive_Easing.cpp
    source/UI/jive/core/jive_Event.cpp
    source/UI/jive/core/jive_Fill.cpp
    source/UI/jive/core/jive_Find.cpp
    source/UI/jive/core/jive_FlexContainer.cpp
    source/UI/jive/core/jive_FlexItem.cpp
    source/UI/jive/core/jive_FlexVariantConverters.cpp
    source/UI/jive/core/jive_FontUtilities.cpp
    source/UI/jive/core/jive_GridContainer.cpp
    source/UI/jive/core/jive_GridItem.cpp
    source/UI/jive/core/jive_GridVariantConverters.cpp
    source/UI/jive/core/jive_Gradient.cpp
    source/UI/jive/core/jive_GuiItem.cpp
    source/UI/jive/core/jive_GuiItemDecorator.cpp
    source/UI/jive/core/jive_IgnoredComponent.cpp
    source/UI/jive/core/jive_Interpolate.cpp
    source/UI/jive/core/jive_Interpreter.cpp
    source/UI/jive/core/jive_Label.cpp
    source/UI/jive/core/jive_Length.cpp
    source/UI/jive/core/jive_NormalisedProgressBar.cpp
    source/UI/jive/core/jive_Object.cpp
    source/UI/jive/core/jive_Orientation.cpp
    source/UI/jive/core/jive_Overflow.cpp
    source/UI/jive/core/jive_ProgressBar.cpp
    source/UI/jive/core/jive_Property.cpp
    source/UI/jive/core/jive_Slider.cpp
    source/UI/jive/core/jive_StringStreams.cpp
    source/UI/jive/core/jive_StyleSheet.cpp
    source/UI/jive/core/jive_Text.cpp
    source/UI/jive/core/jive_TextComponent.cpp
    source/UI/jive/core/jive_Timer.cpp
    source/UI/jive/core/jive_Transition.cpp
    source/UI/jive/core/jive_Transitions.cpp
    source/UI/jive/core/jive_VariantConvertion.cpp
    source/UI/jive/core/jive_View.cpp
)

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
    source/Recording/MidiTrackMergeEngine.cpp
    source/Recording/MidiTrackMergeEngine.h
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
    source/UI/ColourSwatchButton.cpp
    source/UI/ColourSwatchButton.h
    source/UI/ViewHost.cpp
    source/UI/ViewHost.h
    source/UI/jive/JiveBuilderHelpers.h
    source/UI/jive/JiveModalDialog.cpp
    source/UI/jive/JiveModalDialog.h
    source/UI/PerformanceMetadataDialog.cpp
    source/UI/PerformanceMetadataDialog.h
    source/UI/PresetDialogs.cpp
    source/UI/PresetDialogs.h
    source/Settings/SettingsStore.h
    source/Settings/SettingsSerialization.cpp
    source/Settings/SettingsSerialization.h
    source/Settings/SettingsComponent.cpp
    source/Settings/SettingsComponent.h
    source/Settings/jive/SettingsLayoutModel.cpp
    source/Settings/jive/SettingsLayoutModel.h
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
    source/UI/jive/DesignTokens.cpp
    source/UI/jive/DesignTokens.h
    source/UI/jive/StyleCatalog.cpp
    source/UI/jive/StyleCatalog.h
    source/UI/jive/LayoutModel.cpp
    source/UI/jive/LayoutModel.h
    source/UI/jive/JiveModalDialog.cpp
    source/UI/jive/JiveModalDialog.h
    source/UI/jive/JiveUtils.h
    source/UI/jive/StyleBootstrap.h
    source/UI/jive/JiveComponentRegistry.h
    ${DEVPIANO_JIVE_CORE_SOURCES}
)
# ========================================================
# 4.1 静态资产二进制打包（JIVE 样式表、设计 Token 与语言包）
# ========================================================
juce_add_binary_data(devpiano_binary_data
    SOURCES
        source/UI/jive/design_tokens.json
        source/UI/jive/style_sheets.json
        source/Locale/zh_CN.loc
)

# 包含 source 目录作为头文件查找路径，这样代码里可以通过 #include "xxx.h" 直接引用
target_include_directories(devpiano PRIVATE source source/UI/jive/core)

juce_generate_juce_header(devpiano)

# ========================================================
# 5. 链接 JUCE 模块
# ========================================================
target_link_libraries(devpiano PRIVATE
    devpiano_binary_data
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
    NOMINMAX=1
    WIN32_LEAN_AND_MEAN=1

    # 核心：允许我们的应用加载外部 VST3 插件（VST3 插件音源）
    # JIVE：同时使用 jive_layouts 和 jive_style_sheets 时推荐定义
    JIVE_GUI_ITEMS_HAVE_STYLE_SHEETS=1
    JUCE_PLUGINHOST_VST3=1

    JIVE_ENABLE_GRID=1
    # 如果你也想加载古老的 VST2 插件，可以解开下方的注释
    # 但前提是你必须持有 Steinberg 的旧版 VST2 SDK 并在 CMake 中配置路径
    # JUCE_PLUGINHOST_VST=1
)

# 预编译头 (PCH) 优化：大幅减少重复解析大型标准库高频头文件的 CPU 开销
target_precompile_headers(devpiano PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source/pch.h")

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
            # ADR-011 决策 2：与 devpiano 相同的 PCH/ccache 时间戳协同
            -Xclang
            -fno-pch-timestamp
        )
    endif()
    if (MSVC)
        target_compile_options(devpiano_tests PRIVATE
            /utf-8
            /permissive-
            /Zc:__cplusplus
            /W4
            # JUCE 模块 .c 文件使 target 含 C 语言，CMake 为 C 生成 PCH 桩；
            # pch.h 为 STL-only（ADR-011），C 模式下为空翻译单元触发 C4206，无害；
            # JUCE 第三方 C 模块（如 SheenBidi SBBase.c）在 Release 优化下未引用内部静态函数触发 C4505，无害
            $<$<COMPILE_LANGUAGE:C>:/wd4206 /wd4505>
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
        source/Recording/MidiTrackMergeEngine.cpp
        source/Recording/MidiTrackMergeEngine.h
        source/Recording/WavFileExporter.cpp
        source/Recording/WavFileExporter.h
        source/Recording/PluginOfflineRenderer.cpp
        source/Recording/PluginOfflineRenderer.h
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
        source/Export/WavExportTask.cpp
        source/Export/WavExportTask.h

        source/Midi/MidiChannelMapper.cpp
        source/Midi/MidiChannelMapper.h
        source/Audio/AudioEngine.cpp
        source/Audio/AudioEngine.h
        source/Audio/SineSynthVoice.h
        source/Audio/PianoSynthVoice.h
        source/Plugin/PluginHost.cpp
        source/Plugin/PluginHost.h
        source/Plugin/PluginFlowSupport.cpp
        source/Plugin/PluginFlowSupport.h
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
        source/UI/native/AdsrCurveComponent.cpp
        source/UI/native/AdsrCurveComponent.h

        source/UI/jive/StyleCatalog.cpp
        source/UI/jive/StyleCatalog.h
        source/UI/jive/LayoutModel.cpp
        source/UI/jive/LayoutModel.h
        source/UI/jive/JiveBuilderHelpers.h
        source/UI/jive/JiveModalDialog.cpp
        source/UI/jive/JiveModalDialog.h
        source/UI/ViewHost.cpp
        source/UI/ViewHost.h
        source/UI/jive/JiveUtils.h
        source/UI/jive/DesignTokens.cpp
        source/UI/jive/DesignTokens.h
        source/UI/DevPianoLookAndFeel.cpp
        source/UI/DevPianoLookAndFeel.h
        source/UI/PresetDialogs.cpp
        source/UI/PresetDialogs.h
        source/UI/PerformanceMetadataDialog.cpp
        source/UI/PerformanceMetadataDialog.h
        source/UI/KeyBindingEditDialog.cpp
        source/UI/KeyBindingEditDialog.h
        source/UI/ColourSwatchButton.cpp
        source/UI/ColourSwatchButton.h
        source/Settings/SettingsComponent.cpp
        source/Settings/SettingsComponent.h
        source/Settings/jive/SettingsLayoutModel.cpp
        source/Settings/jive/SettingsLayoutModel.h


        # 测试代码
        source/tests/TestRunner.cpp
        source/tests/TestHelpers.h
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
        source/tests/SineSynthVoiceTest.cpp
        source/tests/AppStateAndSerializationTest.cpp
        source/tests/PluginOfflineRendererTest.cpp
        source/tests/PluginOperationControllerTest.cpp
        source/tests/JiveModalDialogTest.cpp
        source/tests/SettingsLayoutModelTest.cpp
        source/tests/ViewHostTest.cpp
        ${DEVPIANO_JIVE_CORE_SOURCES}
    )
    target_link_libraries(devpiano_tests PRIVATE
        devpiano_binary_data
        juce::juce_core
        juce::juce_events
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_audio_basics
        juce::juce_audio_formats
        juce::juce_audio_devices
        juce::juce_audio_processors
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
    )

    target_include_directories(devpiano_tests PRIVATE source source/UI/jive/core)
    juce_generate_juce_header(devpiano_tests)
    target_compile_definitions(devpiano_tests PRIVATE
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_PLUGINHOST_VST3=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        NOMINMAX=1
        WIN32_LEAN_AND_MEAN=1
        JUCE_UNIT_TESTS=1
        JIVE_GUI_ITEMS_HAVE_STYLE_SHEETS=1
        JIVE_ENABLE_GRID=1
    )
    if (MSVC)
        set_target_properties(devpiano_tests PROPERTIES
            MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>"
        )
    endif()
    target_precompile_headers(devpiano_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source/pch.h")

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
                if(_CC_FILE MATCHES "${CMAKE_SOURCE_DIR}/source/" AND NOT _CC_FILE MATCHES "${CMAKE_SOURCE_DIR}/source/UI/jive/(core|extensions)/")
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
