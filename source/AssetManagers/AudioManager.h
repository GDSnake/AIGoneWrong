#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <SDL3_mixer/SDL_mixer.h>

enum class AudioTrack : uint8_t
{
    BGM = 0,
    Room,
    // ADD as needed for the map
    Total
};

static std::string toString(const AudioTrack track)
{
    switch (track)
    {
    case AudioTrack::BGM:
        return "bgm";
    case AudioTrack::Room:
        return "room_music";
    default:
        return "";
    }
}

class AudioManager
{
public:
    static AudioManager& getInstance()
    {
        static AudioManager instance;
        return instance;
    }

    void toggleMusic() const;

    void playMusic(AudioTrack music = AudioTrack::BGM) const;
    void stopMusic() const;
    void resumeMusic() const;
    void pauseMusic() const;
    void shutdown();

private:
    AudioManager();

    ~AudioManager();
	std::unique_ptr<MIX_Mixer, decltype(&MIX_DestroyMixer)> _mixer;
    mutable std::unordered_map<AudioTrack, std::unique_ptr<MIX_Audio, decltype(&MIX_DestroyAudio)>> _music;
	std::unique_ptr <MIX_Track, decltype(&MIX_DestroyTrack)> _track;
    mutable AudioTrack _activeMusic = AudioTrack::Total;
    bool _mixInitialized = false;
};