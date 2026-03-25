#include "SongEngine.h"

void SongEngine::startRecording()
{
    // initialize recording
    songTimer = 0.0;
    playPosition = nullptr;
    recordPosition = eventBuffer;
    songEnd = eventBuffer;
}

void SongEngine::stopRecording()
{
    recordPosition = nullptr;
}

void SongEngine::startPlayback()
{
    if (songEnd && songEnd > eventBuffer) {
        playPosition = eventBuffer;
        songTimer = 0.0;
    }
}

void SongEngine::stopPlayback()
{
    playPosition = nullptr;
}

void SongEngine::sendEvent(uint8_t a, uint8_t b, uint8_t c, uint8_t d, bool record)
{
    if (record && recordPosition) {
        recordPosition->time = songTimer;
        recordPosition->a = a;
        recordPosition->b = b;
        recordPosition->c = c;
        recordPosition->d = d;
        ++recordPosition;
        songEnd = recordPosition;
    }

    // Forward event processing to MIDI output or internal handler (to be implemented)
}

void SongEngine::update(double timeElapsed)
{
    if (playPosition && songEnd) {
        songTimer += timeElapsed * playSpeed;

        while (playPosition < songEnd && playPosition->time <= songTimer) {
            sendEvent(playPosition->a, playPosition->b, playPosition->c, playPosition->d, false);
            ++playPosition;
        }

        if (playPosition >= songEnd) {
            stopPlayback();
        }
    }
}
