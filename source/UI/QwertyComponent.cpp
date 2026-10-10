#include "UI/QwertyComponent.h"

#include "UI/jive/DesignTokens.h"
#include <juce_graphics/juce_graphics.h>

namespace devpiano::ui {

QwertyComponent::QwertyComponent() {
    setOpaque(false);
    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);

    viewModel = devpiano::core::makeDefaultQwertyLayoutTemplate();
    for (std::size_t r = 0; r < 5; ++r) {
        keyGeometries[r].resize(viewModel.rows[r].keys.size());
        numpadGeometries[r].resize(viewModel.numpadRows[r].keys.size());
    }
    setSize(700, 140);
    recalculateKeyBounds();
}

QwertyComponent::~QwertyComponent() {
    stopTimer();
}

void QwertyComponent::updateViewModel(const devpiano::core::QwertyViewModel& newModel) {
    const auto visibilityChanged = viewModel.showNumpad != newModel.showNumpad;
    viewModel = newModel;
    bool needsTimer = false;
    bool sizeChanged = visibilityChanged;

    if (newModel.detectedChord.isValid && newModel.detectedChord.quality != devpiano::core::ChordQuality::unknown) {
        lastDisplayedChord = newModel.detectedChord;
        chordFadeAlpha = 1.0f;
    } else if (chordFadeAlpha > 0.01f) {
        needsTimer = true;
    }
    for (std::size_t r = 0; r < 5; ++r) {
        if (keyGeometries[r].size() != viewModel.rows[r].keys.size()) {
            keyGeometries[r].resize(viewModel.rows[r].keys.size());
            sizeChanged = true;
        }

        for (std::size_t k = 0; k < viewModel.rows[r].keys.size(); ++k) {
            if (viewModel.rows[r].keys[k].isDown) {
                keyGeometries[r][k].fadeAlpha = 1.0f;
            } else if (keyGeometries[r][k].fadeAlpha > 0.01f) {
                needsTimer = true;
            }
        }

        if (numpadGeometries[r].size() != viewModel.numpadRows[r].keys.size()) {
            numpadGeometries[r].resize(viewModel.numpadRows[r].keys.size());
            sizeChanged = true;
        }
        for (std::size_t k = 0; k < viewModel.numpadRows[r].keys.size(); ++k) {
            if (viewModel.numpadRows[r].keys[k].isDown) {
                numpadGeometries[r][k].fadeAlpha = 1.0f;
            } else if (numpadGeometries[r][k].fadeAlpha > 0.01f) {
                needsTimer = true;
            }
        }
    }

    if (sizeChanged) {
        recalculateKeyBounds();
    }

    if (needsTimer && !isTimerRunning()) {
        startTimer(timerIntervalMs);
    }

    repaint();
}

void QwertyComponent::recalculateKeyBounds() {
    const auto bounds = getLocalBounds().toFloat();
    if (bounds.getWidth() < 50.0f || bounds.getHeight() < 50.0f) {
        return;
    }

    constexpr float paddingX = 4.0f;
    constexpr float paddingY = 4.0f;
    constexpr float gapX = 3.0f;
    constexpr float gapY = 3.0f;
    constexpr float numpadGap = 16.0f;

    const auto availableHeight = bounds.getHeight() - (paddingY * 2.0f) - (gapY * 4.0f);
    const auto rowHeight = std::max(12.0f, availableHeight / 5.0f);

    const bool hasNumpad = viewModel.showNumpad;
    constexpr float minUnitW = 44.0f;
    const float minMainW = (15.0f * minUnitW) + (13.0f * gapX);
    const float minNumpadW = hasNumpad ? ((4.0f * minUnitW) + (3.0f * gapX)) : 0.0f;
    const float minTotalW = (paddingX * 2.0f) + minMainW + (hasNumpad ? (numpadGap + minNumpadW) : 0.0f);

    float layoutWidth = bounds.getWidth();
    if (layoutWidth < minTotalW) {
        maxScrollOffset = static_cast<int>(std::ceil(minTotalW - layoutWidth));
        layoutWidth = minTotalW;
        scrollOffsetX = juce::jlimit(0, maxScrollOffset, scrollOffsetX);
    } else {
        maxScrollOffset = 0;
        scrollOffsetX = 0;
    }

    float mainWidth = layoutWidth - (paddingX * 2.0f);
    float numpadWidth = 0.0f;
    if (hasNumpad) {
        const float usable = mainWidth - numpadGap;
        const float unit = usable / 19.0f;
        mainWidth = unit * 15.0f;
        numpadWidth = unit * 4.0f;
    }

    // 1. Position Main Keyboard rows
    for (std::size_t r = 0; r < 5; ++r) {
        const auto& rowKeys = viewModel.rows[r].keys;
        const auto numKeys = rowKeys.size();
        if (numKeys == 0) {
            continue;
        }

        if (keyGeometries[r].size() != numKeys) {
            keyGeometries[r].resize(numKeys);
        }

        const auto y = paddingY + static_cast<float>(r) * (rowHeight + gapY);
        const auto totalGaps = static_cast<float>(numKeys - 1) * gapX;
        const auto availableMainRowW = mainWidth - totalGaps;
        const auto unitWidth = std::max(1.0f, availableMainRowW / 15.0f);

        auto currentX = paddingX - static_cast<float>(scrollOffsetX);
        for (std::size_t k = 0; k < numKeys; ++k) {
            const auto keyW = rowKeys[k].widthWeight * unitWidth;
            keyGeometries[r][k].bounds = juce::Rectangle<float>(currentX, y, keyW, rowHeight);
            currentX += keyW + gapX;
        }
    }

    // 2. Position Numpad rows if active
    if (hasNumpad) {
        const auto numpadStartX = paddingX + mainWidth + numpadGap - static_cast<float>(scrollOffsetX);
        // 统一 4 列基准宽度，不随该行实际键数变化：纵向双高键所在的列必须与上下行精确对齐，
        // 因此跨列键吸收自身内部间隙，未登记的尾部列留给上方跨行键。
        const auto numpadUnitWidth = std::max(1.0f, (numpadWidth - (3.0f * gapX)) / 4.0f);
        for (std::size_t r = 0; r < 5; ++r) {
            const auto& rowKeys = viewModel.numpadRows[r].keys;
            const auto numKeys = rowKeys.size();
            if (numKeys == 0) {
                continue;
            }

            if (numpadGeometries[r].size() != numKeys) {
                numpadGeometries[r].resize(numKeys);
            }

            const auto y = paddingY + static_cast<float>(r) * (rowHeight + gapY);
            auto currentX = numpadStartX;
            for (std::size_t k = 0; k < numKeys; ++k) {
                const auto columnWeight = std::max(1.0f, rowKeys[k].widthWeight);
                const auto rowSpan = std::max(1.0f, rowKeys[k].heightWeight);
                const auto keyW = (columnWeight * numpadUnitWidth) + ((columnWeight - 1.0f) * gapX);
                const auto keyH = (rowSpan * rowHeight) + ((rowSpan - 1.0f) * gapY);
                numpadGeometries[r][k].bounds = juce::Rectangle<float>(currentX, y, keyW, keyH);
                currentX += keyW + gapX;
            }
        }
    }
}

void QwertyComponent::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
    juce::ignoreUnused(e);
    if (maxScrollOffset > 0) {
        const float delta = (wheel.deltaX != 0.0f) ? wheel.deltaX : -wheel.deltaY;
        scrollOffsetX = juce::jlimit(0, maxScrollOffset, scrollOffsetX - static_cast<int>(delta * 80.0f));
        recalculateKeyBounds();
        repaint();
    }
}

void QwertyComponent::resized() {
    recalculateKeyBounds();
}

void QwertyComponent::paint(juce::Graphics& g) {
    constexpr float cornerRadius = 4.0f;
    const auto bullet = juce::String::charToString(0x2022);

    for (std::size_t r = 0; r < 5; ++r) {
        const auto& rowKeys = viewModel.rows[r].keys;
        for (std::size_t k = 0; k < rowKeys.size(); ++k) {
            const auto& keyState = rowKeys[k];
            const auto& geom = keyGeometries[r][k];
            if (geom.bounds.isEmpty()) {
                continue;
            }

            auto rect = geom.bounds;
            const auto isShiftKey = (keyState.mainLabel == "Shift");
            const auto isAltKey = (keyState.mainLabel == "Alt");
            const auto isCtrlKey = (keyState.mainLabel == "Ctrl");
            const auto isModifierActive = (isShiftKey && viewModel.isShiftActive) || (isAltKey && viewModel.isAltActive)
                || (isCtrlKey && viewModel.isCtrlActive);

            const auto isPressed = keyState.isDown || isModifierActive;
            const auto alpha = geom.fadeAlpha;

            if (isPressed) {
                rect = rect.translated(0.0f, 1.5f);
            }
            const auto hasNote = (keyState.mappedMidiNote >= 0);
            const auto hasPedal = (keyState.isSustainPedal || keyState.isSoftPedal || keyState.isSostenutoPedal);

            juce::Colour activeColour(0xFF64748B);
            if (!keyState.isControlOnly) {
                if (hasNote) {
                    activeColour
                        = devpiano::core::getPitchClassHarmonyColour(keyState.mappedMidiNote, 0.84f, 0.96f, 1.0f);
                } else if (keyState.isSustainPedal || (isShiftKey && !keyState.isSoftPedal)) {
                    activeColour = juce::Colour(0xFFF59E0B);
                } else if (keyState.isSoftPedal) {
                    activeColour = juce::Colour(0xFF10B981);
                } else if (keyState.isSostenutoPedal) {
                    activeColour = juce::Colour(0xFF3B82F6);
                } else if (isAltKey) {
                    activeColour = juce::Colour(0xFF38BDF8);
                } else if (isCtrlKey) {
                    activeColour = juce::Colour(0xFFA855F7);
                }
            }

            juce::Colour bgColour;
            juce::Colour borderColour;
            juce::Colour primaryTextColour;
            juce::Colour secondaryTextColour;

            if (isPressed) {
                bgColour = activeColour;
                borderColour = activeColour.brighter(0.35f);
                primaryTextColour = devpiano::core::getContrastingTextColour(activeColour);
                secondaryTextColour = primaryTextColour.withAlpha(0.85f);
            } else if (alpha > 0.01f) {
                bgColour = juce::Colour(0xFF24262B).interpolatedWith(activeColour, alpha * 0.82f);
                borderColour = juce::Colour(0xFF333842).interpolatedWith(activeColour.brighter(0.25f), alpha);
                primaryTextColour = juce::Colours::white;
                secondaryTextColour = activeColour.interpolatedWith(juce::Colours::white, 0.65f);
            } else {
                bgColour = juce::Colour(0xFF24262B); // Zinc 800 dark grey
                borderColour = juce::Colour(0xFF333842);
                primaryTextColour = juce::Colour(0xFFE2E8F0); // Text primary
                if (hasNote) {
                    // Subtle harmonic colour tint for resting note labels
                    secondaryTextColour = activeColour.interpolatedWith(juce::Colour(0xFFCBD5E1), 0.45f);
                } else if (keyState.isSustainPedal) {
                    if (viewModel.isSyncPedalCutPending) {
                        borderColour = juce::Colour(0xFFF59E0B).withAlpha(0.85f);
                        secondaryTextColour = juce::Colour(0xFFF59E0B);
                    } else {
                        secondaryTextColour = activeColour.withAlpha(0.85f);
                    }
                } else if (keyState.isSoftPedal) {
                    secondaryTextColour = activeColour.withAlpha(0.85f);
                } else {
                    secondaryTextColour = juce::Colour(0xFF94A3B8); // Muted slate
                }
            }
            // Fill key background
            g.setColour(bgColour);
            g.fillRoundedRectangle(rect, cornerRadius);

            // Draw border
            g.setColour(borderColour);
            g.drawRoundedRectangle(rect.reduced(0.5f), cornerRadius, 1.0f);

            if (hasNote || hasPedal) {
                // Two-tier text display
                const auto topRect = rect.withTrimmedBottom(rect.getHeight() * 0.45f);
                const auto bottomRect = rect.withTrimmedTop(rect.getHeight() * 0.45f);

                g.setColour(primaryTextColour);
                g.setFont(devpiano::jive::DesignTokens::getUnifiedUiFont(11.5f, juce::Font::bold));
                g.drawFittedText(keyState.mainLabel, topRect.toNearestInt(), juce::Justification::centred, 1);

                g.setColour(secondaryTextColour);
                g.setFont(devpiano::jive::DesignTokens::getUnifiedUiFont(9.5f));
                juce::String noteLabel;
                if (hasNote) {
                    noteLabel = keyState.noteName + " " + bullet + " " + keyState.solfegeLabel;
                } else if (keyState.isSustainPedal) {
                    if (isPressed) {
                        noteLabel = (viewModel.sustainPolicy == devpiano::core::SustainPolicy::syncPedal)
                            ? "[Sync Hold]"
                            : "[Sustain]";
                    } else if (viewModel.isSyncPedalCutPending) {
                        noteLabel = "[Sync Cut]";
                    } else {
                        noteLabel = (viewModel.sustainPolicy == devpiano::core::SustainPolicy::syncPedal)
                            ? "[Sync Pedal]"
                            : "[Direct Pedal]";
                    }
                } else if (keyState.isSoftPedal) {
                    noteLabel = "[Soft]";
                } else if (keyState.isSostenutoPedal) {
                    noteLabel = isPressed ? "[Sost Hold]" : "[Sost CC66]";
                }
                g.drawFittedText(noteLabel, bottomRect.toNearestInt(), juce::Justification::centred, 1);
            } else {
                // Single-tier text display (function keys like Caps, Enter, Ctrl, etc.)
                g.setColour(primaryTextColour);
                g.setFont(devpiano::jive::DesignTokens::getUnifiedUiFont(11.0f, juce::Font::bold));
                juce::String functionLabel = keyState.mainLabel;
                if (isShiftKey) {
                    functionLabel = viewModel.isShiftActive ? "Shift [BOOST]" : "Shift";
                } else if (isAltKey) {
                    functionLabel = viewModel.isAltActive ? "Alt [+8va]" : "Alt";
                } else if (isCtrlKey) {
                    functionLabel = viewModel.isCtrlActive ? "Ctrl [MOD]" : "Ctrl";
                }
                g.drawFittedText(functionLabel, rect.toNearestInt(), juce::Justification::centred, 1);
            }
        }
    }

    // -- Render Numpad Rows (Phase 38-3) --
    if (viewModel.showNumpad) {
        for (std::size_t r = 0; r < 5; ++r) {
            const auto& rowKeys = viewModel.numpadRows[r].keys;
            for (std::size_t k = 0; k < rowKeys.size(); ++k) {
                const auto& keyState = rowKeys[k];
                const auto& geom = numpadGeometries[r][k];
                if (geom.bounds.isEmpty()) {
                    continue;
                }

                auto rect = geom.bounds;
                const auto isPressed = keyState.isDown;
                const auto alpha = geom.fadeAlpha;
                if (isPressed) {
                    rect = rect.translated(0.0f, 1.5f);
                }
                const auto hasNote = (keyState.mappedMidiNote >= 0) && !keyState.isControlOnly;

                juce::Colour activeColour;
                if (keyState.isControlOnly) {
                    activeColour = juce::Colour(0xFF475569);
                } else if (hasNote) {
                    activeColour
                        = devpiano::core::getPitchClassHarmonyColour(keyState.mappedMidiNote, 0.84f, 0.96f, 1.0f);
                } else {
                    activeColour = juce::Colour(0xFF64748B);
                }

                juce::Colour bgColour;
                juce::Colour borderColour;
                juce::Colour primaryTextColour;
                juce::Colour secondaryTextColour;

                if (isPressed) {
                    bgColour = activeColour;
                    borderColour = activeColour.brighter(0.35f);
                    primaryTextColour = devpiano::core::getContrastingTextColour(activeColour);
                    secondaryTextColour = primaryTextColour.withAlpha(0.85f);
                } else if (alpha > 0.01f) {
                    bgColour = juce::Colour(0xFF24262B).interpolatedWith(activeColour, alpha * 0.82f);
                    borderColour = juce::Colour(0xFF333842).interpolatedWith(activeColour.brighter(0.25f), alpha);
                    primaryTextColour = juce::Colours::white;
                    secondaryTextColour = activeColour.interpolatedWith(juce::Colours::white, 0.65f);
                } else {
                    bgColour = juce::Colour(0xFF24262B);
                    borderColour = juce::Colour(0xFF333842);
                    primaryTextColour = juce::Colour(0xFFE2E8F0);
                    secondaryTextColour = hasNote ? activeColour.interpolatedWith(juce::Colour(0xFFCBD5E1), 0.45f)
                                                  : juce::Colour(0xFF94A3B8);
                }

                g.setColour(bgColour);
                g.fillRoundedRectangle(rect, cornerRadius);
                g.setColour(borderColour);
                g.drawRoundedRectangle(rect.reduced(0.5f), cornerRadius, 1.0f);

                if (hasNote) {
                    // 双高键（如 Num +）把两级文本作为整体垂直居中，避免标签被拉伸到键体上下两端
                    auto textArea = rect;
                    if (keyState.heightWeight > 1.0f) {
                        textArea
                            = rect.withSizeKeepingCentre(rect.getWidth(), rect.getHeight() / keyState.heightWeight);
                    }
                    const auto topRect = textArea.withTrimmedBottom(textArea.getHeight() * 0.45f);
                    const auto bottomRect = textArea.withTrimmedTop(textArea.getHeight() * 0.45f);
                    g.setColour(primaryTextColour);
                    g.setFont(devpiano::jive::DesignTokens::getUnifiedUiFont(11.5f, juce::Font::bold));
                    g.drawFittedText(keyState.mainLabel, topRect.toNearestInt(), juce::Justification::centred, 1);

                    g.setColour(secondaryTextColour);
                    g.setFont(devpiano::jive::DesignTokens::getUnifiedUiFont(9.5f));
                    const auto noteLabel = keyState.noteName + " " + bullet + " " + keyState.solfegeLabel;
                    g.drawFittedText(noteLabel, bottomRect.toNearestInt(), juce::Justification::centred, 1);
                } else {
                    g.setColour(primaryTextColour);
                    g.setFont(devpiano::jive::DesignTokens::getUnifiedUiFont(11.0f, juce::Font::bold));
                    g.drawFittedText(keyState.mainLabel, rect.toNearestInt(), juce::Justification::centred, 1);
                }
            }
        }
    }

    // -- Zone Partition Dividers & Badges (Phase 38-3) --
    if (viewModel.partitionMode == devpiano::core::KeyboardPartitionMode::mainOnly) {
        // MainOnly: Area A (left: 1~5, Q~T, A~G, Z~B) vs Area B (right: 6~0, Y~P, H~L, N~M)
        // Stepped divider path tracing physical gaps between keys and truncated above Spacebar (Row 4).
        if (keyGeometries[0].size() >= 7 && keyGeometries[1].size() >= 7 && keyGeometries[2].size() >= 7
            && keyGeometries[3].size() >= 7) {
            const float x0 = (keyGeometries[0][5].bounds.getRight() + keyGeometries[0][6].bounds.getX()) * 0.5f;
            const float x1 = (keyGeometries[1][5].bounds.getRight() + keyGeometries[1][6].bounds.getX()) * 0.5f;
            const float x2 = (keyGeometries[2][5].bounds.getRight() + keyGeometries[2][6].bounds.getX()) * 0.5f;
            const float x3 = (keyGeometries[3][5].bounds.getRight() + keyGeometries[3][6].bounds.getX()) * 0.5f;

            const float yTop = keyGeometries[0][5].bounds.getY();
            const float yMid01 = (keyGeometries[0][5].bounds.getBottom() + keyGeometries[1][5].bounds.getY()) * 0.5f;
            const float yMid12 = (keyGeometries[1][5].bounds.getBottom() + keyGeometries[2][5].bounds.getY()) * 0.5f;
            const float yMid23 = (keyGeometries[2][5].bounds.getBottom() + keyGeometries[3][5].bounds.getY()) * 0.5f;
            const float yBottom = keyGeometries[3][5].bounds.getBottom();

            juce::Path dividerPath;
            dividerPath.startNewSubPath(x0, yTop);
            dividerPath.lineTo(x0, yMid01);
            dividerPath.lineTo(x1, yMid01);
            dividerPath.lineTo(x1, yMid12);
            dividerPath.lineTo(x2, yMid12);
            dividerPath.lineTo(x2, yMid23);
            dividerPath.lineTo(x3, yMid23);
            dividerPath.lineTo(x3, yBottom);

            const auto smoothedPath = dividerPath.createPathWithRoundedCorners(4.0f);
            g.setColour(juce::Colour(0x8038BDF8)); // Subtle sky blue divider
            g.strokePath(smoothedPath,
                         juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    // ── Real-time Chord Recognition HUD Badge ───────────────────────────────
    if (lastDisplayedChord.isValid && chordFadeAlpha > 0.01f) {
        const auto bounds = getLocalBounds().toFloat();
        constexpr float badgeW = 148.0f;
        constexpr float badgeH = 24.0f;
        const auto badgeX = bounds.getRight() - badgeW - 6.0f;
        constexpr float badgeY = 5.0f;
        const juce::Rectangle<float> badgeRect(badgeX, badgeY, badgeW, badgeH);

        const auto rootColour
            = devpiano::core::getPitchClassHarmonyColour(lastDisplayedChord.rootPitchClass, 0.85f, 0.95f, 1.0f);

        // Glassmorphism background pill
        g.setColour(juce::Colour(0xEB0A0F1D).withAlpha(0.85f * chordFadeAlpha));
        g.fillRoundedRectangle(badgeRect, 5.0f);

        // 12-TET Harmony subtle tint
        g.setColour(rootColour.withAlpha(0.20f * chordFadeAlpha));
        g.fillRoundedRectangle(badgeRect, 5.0f);

        // Harmony border stroke
        g.setColour(rootColour.withAlpha(0.75f * chordFadeAlpha));
        g.drawRoundedRectangle(badgeRect, 5.0f, 1.0f);

        // Indicator dot
        constexpr float dotSize = 6.0f;
        const juce::Rectangle<float> dotRect(badgeX + 8.0f, badgeY + (badgeH - dotSize) * 0.5f, dotSize, dotSize);
        g.setColour(rootColour.withAlpha(chordFadeAlpha));
        g.fillEllipse(dotRect);

        // Chord name (main text)
        g.setColour(juce::Colours::white.withAlpha(chordFadeAlpha));
        g.setFont(devpiano::jive::DesignTokens::getUnifiedUiFont(13.0f, juce::Font::bold));
        const juce::Rectangle<float> nameRect(badgeX + 18.0f, badgeY + 1.0f, 74.0f, badgeH - 2.0f);
        g.drawFittedText(lastDisplayedChord.chordName, nameRect.toNearestInt(), juce::Justification::centredLeft, 1);

        // Inversion / Quality subtitle
        g.setColour(juce::Colour(0xFF94A3B8).withAlpha(chordFadeAlpha));
        g.setFont(devpiano::jive::DesignTokens::getUnifiedUiFont(9.5f, juce::Font::plain));
        const juce::Rectangle<float> descRect(badgeX + 90.0f, badgeY + 1.0f, badgeW - 94.0f, badgeH - 2.0f);
        const auto descText = (lastDisplayedChord.inversion != devpiano::core::ChordInversion::rootPosition)
            ? lastDisplayedChord.inversionDescription
            : lastDisplayedChord.qualityDescription;
        g.drawFittedText(descText, descRect.toNearestInt(), juce::Justification::centredRight, 1);
    }
}

void QwertyComponent::timerCallback() {
    bool hasActiveFade = false;

    for (std::size_t r = 0; r < 5; ++r) {
        for (std::size_t k = 0; k < keyGeometries[r].size(); ++k) {
            auto& geom = keyGeometries[r][k];
            if (!viewModel.rows[r].keys[k].isDown && geom.fadeAlpha > 0.005f) {
                geom.fadeAlpha *= fadeDecayFactor;
                hasActiveFade = true;
            } else if (!viewModel.rows[r].keys[k].isDown) {
                geom.fadeAlpha = 0.0f;
            }
        }
        for (std::size_t k = 0; k < numpadGeometries[r].size(); ++k) {
            auto& geom = numpadGeometries[r][k];
            if (!viewModel.numpadRows[r].keys[k].isDown && geom.fadeAlpha > 0.005f) {
                geom.fadeAlpha *= fadeDecayFactor;
                hasActiveFade = true;
            } else if (!viewModel.numpadRows[r].keys[k].isDown) {
                geom.fadeAlpha = 0.0f;
            }
        }
    }
    if (viewModel.detectedChord.isValid && viewModel.detectedChord.quality != devpiano::core::ChordQuality::unknown) {
        chordFadeAlpha = 1.0f;
    } else if (chordFadeAlpha > 0.005f) {
        chordFadeAlpha *= chordFadeDecayFactor;
        hasActiveFade = true;
    } else {
        chordFadeAlpha = 0.0f;
    }

    if (!hasActiveFade) {
        stopTimer();
    }

    repaint();
}

QwertyComponent::HitResult QwertyComponent::findKeyAt(juce::Point<int> position) const {
    const auto pos = position.toFloat();
    for (std::size_t r = 0; r < 5; ++r) {
        for (std::size_t k = 0; k < keyGeometries[r].size(); ++k) {
            if (keyGeometries[r][k].bounds.contains(pos)) {
                return { static_cast<int>(r), static_cast<int>(k), false, &viewModel.rows[r].keys[k] };
            }
        }
    }
    if (viewModel.showNumpad) {
        for (std::size_t r = 0; r < 5; ++r) {
            for (std::size_t k = 0; k < numpadGeometries[r].size(); ++k) {
                if (numpadGeometries[r][k].bounds.contains(pos)) {
                    return { static_cast<int>(r), static_cast<int>(k), true, &viewModel.numpadRows[r].keys[k] };
                }
            }
        }
    }
    return {};
}

void QwertyComponent::mouseDown(const juce::MouseEvent& e) {
    const auto hit = findKeyAt(e.getPosition());
    if (hit.key == nullptr) {
        return;
    }

    if (e.mods.isPopupMenu()) {
        if (hit.key->mappedMidiNote >= 0 && onBindingEditRequested != nullptr) {
            onBindingEditRequested(hit.key->bindingMidiNote, hit.key->keyCode);
        }
        return;
    }

    if (hit.key->mappedMidiNote >= 0) {
        lastMouseDownNote = hit.key->mappedMidiNote;
        lastMouseDownKeyCode = hit.key->keyCode;
        if (hit.isNumpad) {
            numpadGeometries[static_cast<std::size_t>(hit.rowIndex)][static_cast<std::size_t>(hit.keyIndex)].fadeAlpha
                = 1.0f;
        } else {
            keyGeometries[static_cast<std::size_t>(hit.rowIndex)][static_cast<std::size_t>(hit.keyIndex)].fadeAlpha
                = 1.0f;
        }
        if (!isTimerRunning()) {
            startTimer(timerIntervalMs);
        }
        repaint();
        if (hit.key->velocity > 0.0f) {
            if (onPhysicalNoteOn != nullptr) {
                lastMouseDownIdentity = onPhysicalNoteOn(hit.key->keyCode, hit.key->inputMidiNote,
                                                         hit.key->inputMidiChannel, hit.key->inputVelocity);
            } else if (onNoteOn != nullptr) {
                lastMouseDownIdentity
                    = onNoteOn(hit.key->inputMidiNote, hit.key->inputMidiChannel, hit.key->inputVelocity);
            }
        }
    }
}

void QwertyComponent::mouseUp(const juce::MouseEvent& e) {
    juce::ignoreUnused(e);
    releaseHeldMouseNote();
}

void QwertyComponent::releaseHeldMouseNote() {
    const auto identity = lastMouseDownIdentity;
    const int code = lastMouseDownKeyCode;
    lastMouseDownNote = -1;
    lastMouseDownKeyCode = 0;
    lastMouseDownIdentity.reset();
    if (identity.has_value()) {
        if (onPhysicalNoteOff != nullptr) {
            onPhysicalNoteOff(*identity, code);
        } else if (onNoteOff != nullptr) {
            onNoteOff(*identity);
        }
    }
    repaint();
}

void QwertyComponent::mouseDrag(const juce::MouseEvent& e) {
    const auto hit = findKeyAt(e.getPosition());
    if (hit.key == nullptr || hit.key->mappedMidiNote == lastMouseDownNote) {
        return;
    }

    releaseHeldMouseNote();

    if (hit.key->mappedMidiNote >= 0) {
        lastMouseDownNote = hit.key->mappedMidiNote;
        lastMouseDownKeyCode = hit.key->keyCode;
        if (hit.isNumpad) {
            numpadGeometries[static_cast<std::size_t>(hit.rowIndex)][static_cast<std::size_t>(hit.keyIndex)].fadeAlpha
                = 1.0f;
        } else {
            keyGeometries[static_cast<std::size_t>(hit.rowIndex)][static_cast<std::size_t>(hit.keyIndex)].fadeAlpha
                = 1.0f;
        }
        if (!isTimerRunning()) {
            startTimer(timerIntervalMs);
        }
        repaint();
        if (hit.key->velocity > 0.0f) {
            if (onPhysicalNoteOn != nullptr) {
                lastMouseDownIdentity = onPhysicalNoteOn(hit.key->keyCode, hit.key->inputMidiNote,
                                                         hit.key->inputMidiChannel, hit.key->inputVelocity);
            } else if (onNoteOn != nullptr) {
                lastMouseDownIdentity
                    = onNoteOn(hit.key->inputMidiNote, hit.key->inputMidiChannel, hit.key->inputVelocity);
            }
        }
    }
}

} // namespace devpiano::ui
