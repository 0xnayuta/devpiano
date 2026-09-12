#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace devpiano::input {

/// 触键力度曲线（Phase 29-C, Touch Velocity Curves）：
/// 针对电脑薄膜键盘、不同机械轴体以及外接 MIDI 键盘的阻尼手感与敲击动态自适应映射。
enum class TouchVelocityCurve : std::uint8_t {
    standard = 0, ///< 线性响应 (Linear): v_out = v_in，中性标准传递
    light = 1, ///< 轻触感 (Soft / High Sensitivity): 对中小力度向上凸起补偿 (v^0.65)，轻触即发
    heavy = 2, ///< 重阻尼 (Firm / Low Sensitivity): 对低力度凹陷压制 (v^1.60)，强调深沉触感与 pp 细致控制
    wideDynamic = 3, ///< 宽动态 S 曲线 (Expressive Sigmoid): 放大强弱反差，极弱更柔，强奏爆发力强
};

/// 对输入归一化力度 [0.0, 1.0] 进行曲线映射，严格保证单调递增与端点守恒 [0,1] -> [0,1]。
[[nodiscard]] inline float applyVelocityCurve(float rawVelocity, TouchVelocityCurve curve) noexcept {
    const auto v = std::clamp(rawVelocity, 0.0f, 1.0f);
    if (v <= 0.0f || v >= 1.0f) {
        return v;
    }

    switch (curve) {
    case TouchVelocityCurve::standard:
        return v;

    case TouchVelocityCurve::light:
        // 凸曲线: 指数 0.65f，使得 0.3 提升至约 0.45，0.5 提升至约 0.63
        return std::pow(v, 0.65f);

    case TouchVelocityCurve::heavy:
        // 凹曲线: 指数 1.60f，使得 0.3 压制至约 0.14，0.5 压制至约 0.33
        return std::pow(v, 1.60f);

    case TouchVelocityCurve::wideDynamic:
        // S 型 Sigmoid 平滑三次 Hermite 插值: 3v^2 - 2v^3，两端平缓、中段陡峭
        return v * v * (3.0f - 2.0f * v);
    }

    return v;
}

/// 辅助名称解析
[[nodiscard]] inline const char* touchVelocityCurveToString(TouchVelocityCurve curve) noexcept {
    switch (curve) {
    case TouchVelocityCurve::standard:
        return "Standard";
    case TouchVelocityCurve::light:
        return "Light";
    case TouchVelocityCurve::heavy:
        return "Heavy";
    case TouchVelocityCurve::wideDynamic:
        return "Wide Dynamic";
    }
    return "Standard";
}

} // namespace devpiano::input
