#pragma once

#include <JuceHeader.h>

struct song_event_t {
    double time;
    uint8_t a;
    uint8_t b;
    uint8_t c;
    uint8_t d;
};

class SongEngine
{
public:
    SongEngine() = default;
    ~SongEngine() = default;

    void startRecording();
    void stopRecording();

    void startPlayback();
    void stopPlayback();

    void sendEvent(uint8_t a, uint8_t b, uint8_t c, uint8_t d, bool record = true);
    void update(double timeElapsed);

private:
    static const size_t bufferSize = 1024 * 1024;
    song_event_t eventBuffer[bufferSize];
    song_event_t* playPosition = nullptr;
    song_event_t* recordPosition = nullptr;
    song_event_t* songEnd = nullptr;

    double songTimer = 0.0;
    double playSpeed = 1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SongEngine)
};
