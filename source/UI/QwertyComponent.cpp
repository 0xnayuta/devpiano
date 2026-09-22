#include "UI/QwertyComponent.h"

#include <juce_graphics/juce_graphics.h>

namespace devpiano::ui {

QwertyComponent::QwertyComponent() {
    setOpaque(false);
    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);

    viewModel = devpiano::core::makeDefaultQwertyLayoutTemplate();
    for (std::size_t r = 0; r < 5; ++r) {
        keyGeometries[r].resize(viewModel.rows[r].keys.size());
    }

    setSize(700, 140);
    recalculateKeyBounds();
}

QwertyComponent::~QwertyComponent() {
    stopTimer();
}

void QwertyComponent::updateViewModel(const devpiano::core::QwertyViewModel& newModel) {
    viewModel = newModel;

    bool needsTimer = false;
    bool sizeChanged = false;
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

    const auto availableHeight = bounds.getHeight() - (paddingY * 2.0f) - (gapY * 4.0f);
    const auto rowHeight = std::max(12.0f, availableHeight / 5.0f);

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
        const auto availableWidth = bounds.getWidth() - (paddingX * 2.0f) - totalGaps;
        const auto unitWidth = std::max(1.0f, availableWidth / 15.0f);

        auto currentX = paddingX;
        for (std::size_t k = 0; k < numKeys; ++k) {
            const auto keyW = rowKeys[k].widthWeight * unitWidth;
            keyGeometries[r][k].bounds = juce::Rectangle<float>(currentX, y, keyW, rowHeight);
            currentX += keyW + gapX;
        }
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
            const auto hasPedal = (keyState.isSustainPedal || keyState.isSoftPedal);

            juce::Colour activeColour;
            if (hasNote) {
                activeColour = devpiano::core::getPitchClassHarmonyColour(keyState.mappedMidiNote, 0.84f, 0.96f, 1.0f);
            } else if (keyState.isSustainPedal) {
                activeColour = juce::Colour(0xFFF59E0B); // Amber for sustain pedal
            } else if (keyState.isSoftPedal) {
                activeColour = juce::Colour(0xFF10B981); // Emerald for soft pedal
            } else if (isShiftKey) {
                activeColour = juce::Colour(0xFFF59E0B); // Warm Gold for Velocity Boost
            } else if (isAltKey) {
                activeColour = juce::Colour(0xFF38BDF8); // Sky Blue for Octave Shift
            } else if (isCtrlKey) {
                activeColour = juce::Colour(0xFFA855F7); // Purple for Ctrl
            } else {
                activeColour = juce::Colour(0xFF64748B); // Slate gray for other function keys
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
                g.setFont(juce::FontOptions(11.5f).withStyle("Bold"));
                g.drawFittedText(keyState.mainLabel, topRect.toNearestInt(), juce::Justification::centred, 1);

                g.setColour(secondaryTextColour);
                g.setFont(juce::FontOptions(9.5f));
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
                }
                g.drawFittedText(noteLabel, bottomRect.toNearestInt(), juce::Justification::centred, 1);
            } else {
                // Single-tier text display (function keys like Caps, Enter, Ctrl, etc.)
                g.setColour(primaryTextColour);
                g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
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
                return { static_cast<int>(r), static_cast<int>(k), &viewModel.rows[r].keys[k] };
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
            onBindingEditRequested(hit.key->mappedMidiNote);
        }
        return;
    }

    if (hit.key->mappedMidiNote >= 0 && onNoteOn != nullptr) {
        lastMouseDownNote = hit.key->mappedMidiNote;
        lastMouseDownChannel = hit.key->mappedMidiChannel;
        keyGeometries[static_cast<std::size_t>(hit.rowIndex)][static_cast<std::size_t>(hit.keyIndex)].fadeAlpha = 1.0f;
        if (!isTimerRunning()) {
            startTimer(timerIntervalMs);
        }
        repaint();
        onNoteOn(lastMouseDownNote, lastMouseDownChannel, hit.key->velocity);
    }
}

void QwertyComponent::mouseUp(const juce::MouseEvent& e) {
    juce::ignoreUnused(e);
    releaseHeldMouseNote();
}

void QwertyComponent::releaseHeldMouseNote() {
    if (lastMouseDownNote >= 0 && onNoteOff != nullptr) {
        onNoteOff(lastMouseDownNote, lastMouseDownChannel);
        lastMouseDownNote = -1;
    }
    repaint();
}

void QwertyComponent::mouseDrag(const juce::MouseEvent& e) {
    const auto hit = findKeyAt(e.getPosition());
    if (hit.key == nullptr || hit.key->mappedMidiNote == lastMouseDownNote) {
        return;
    }

    if (lastMouseDownNote >= 0 && onNoteOff != nullptr) {
        onNoteOff(lastMouseDownNote, lastMouseDownChannel);
        lastMouseDownNote = -1;
    }

    if (hit.key->mappedMidiNote >= 0 && onNoteOn != nullptr) {
        lastMouseDownNote = hit.key->mappedMidiNote;
        lastMouseDownChannel = hit.key->mappedMidiChannel;
        keyGeometries[static_cast<std::size_t>(hit.rowIndex)][static_cast<std::size_t>(hit.keyIndex)].fadeAlpha = 1.0f;
        if (!isTimerRunning()) {
            startTimer(timerIntervalMs);
        }
        repaint();
        onNoteOn(lastMouseDownNote, lastMouseDownChannel, hit.key->velocity);
    }
}

} // namespace devpiano::ui
