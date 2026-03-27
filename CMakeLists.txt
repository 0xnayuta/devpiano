cmake_minimum_required(VERSION 3.22)

# 定义项目名称与版本
project(devpiano VERSION 1.0.0)

# ========================================================
# 1. 引入 JUCE 框架
# ========================================================
# 告诉 CMake 将 JUCE 目录作为子项目添加进构建树
add_subdirectory(JUCE)

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
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/MainComponent.h
    Source/Audio/AudioEngine.cpp
    Source/Audio/AudioEngine.h
    Source/Input/KeyboardMidiMapper.cpp
    Source/Input/KeyboardMidiMapper.h
    Source/Core/KeyMapTypes.h
    Source/Core/MidiTypes.h
    Source/Core/AppState.h
    Source/Core/AppStateBuilder.h
    Source/Midi/MidiRouter.cpp
    Source/Midi/MidiRouter.h
    Source/Plugin/PluginHost.cpp
    Source/Plugin/PluginHost.h
    Source/UI/PluginPanel.cpp
    Source/UI/PluginPanel.h
    Source/UI/PluginPanelStateBuilder.cpp
    Source/UI/PluginPanelStateBuilder.h
    Source/UI/ControlsPanel.cpp
    Source/UI/ControlsPanel.h
    Source/UI/HeaderPanel.cpp
    Source/UI/HeaderPanel.h
    Source/UI/HeaderPanelStateBuilder.cpp
    Source/UI/HeaderPanelStateBuilder.h
    Source/UI/KeyboardPanel.cpp
    Source/UI/KeyboardPanel.h
    Source/Settings/SettingsModel.h
    Source/Settings/SettingsStore.cpp
    Source/Settings/SettingsStore.h
    Source/Settings/SettingsComponent.h
)

# 包含 Source 目录作为头文件查找路径，这样代码里可以通过 #include "xxx.h" 直接引用
target_include_directories(devpiano PRIVATE Source)

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
)

# ========================================================
# 6. 配置预编译宏（开启插件宿主支持）
# ========================================================
target_compile_definitions(devpiano PRIVATE
    # 禁用一些不需要的沉重组件以加快编译
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    NOMINMAX=1
    WIN32_LEAN_AND_MEAN=1

    # 核心：允许我们的应用加载外部 VST3 插件（FreePiano 发声必备）
    JUCE_PLUGINHOST_VST3=1

    # 如果你也想加载古老的 VST2 插件，可以解开下方的注释
    # 但前提是你必须持有 Steinberg 的旧版 VST2 SDK 并在 CMake 中配置路径
    # JUCE_PLUGINHOST_VST=1
)

if (MSVC)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT devpiano)
endif()