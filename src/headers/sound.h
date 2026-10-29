// sound.h - WorkSound: the end-of-phase ding chime and the looping work sound.
#ifndef TOMATO_SOUND_H
#define TOMATO_SOUND_H

#include <windows.h>
#include <string>

class WorkSound {
public:
    WorkSound() = default;
    ~WorkSound();   // stops any looping playback

    WorkSound(const WorkSound&) = delete;
    WorkSound& operator=(const WorkSound&) = delete;

    // Set the .wav to loop (empty disables). Does not start playback.
    void setPath(std::wstring path) { path_ = std::move(path); }
    const std::wstring& path() const { return path_; }
    bool hasPath() const { return !path_.empty(); }

    // Start / stop the async loop. start() is a no-op if no path is set.
    void start();
    void stop();
    bool playing() const { return playing_; }

    // Play the short end-of-phase "ding bell" chime (non-blocking). Static
    // because it does not depend on instance state.
    static void playDing();

private:
    std::wstring path_;
    bool         playing_ = false;
};

#endif // TOMATO_SOUND_H
