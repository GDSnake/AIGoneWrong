#include "AudioManager.h"

#include <iostream>

#include "Config/Config.h"

AudioManager::AudioManager(): _mixer(nullptr, MIX_DestroyMixer),
                              _track(nullptr, MIX_DestroyTrack)
{   
    const auto& audio = Config::getInstance().getConfigData().audio;
    if (audio.empty())
    {
        return;
    }

    _mixInitialized = MIX_Init();
    if (!_mixInitialized) {
        std::cerr << "MIX_Init failed: " << SDL_GetError() << '\n';
        return;
    }
    SDL_AudioSpec audioSpec = {SDL_AUDIO_S16, 2, 44100};
    _mixer.reset(MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audioSpec));
    if (!_mixer)
    {
        std::cerr << "Audio Manager Init Error: " << SDL_GetError() << '\n';
        return;
    }

    _track.reset(MIX_CreateTrack(_mixer.get()));
    if (!_track)
    {
        std::cerr << "Audio Manager Track Error: " << SDL_GetError() << '\n';
        return;
    }

}

AudioManager::~AudioManager()
{
    shutdown();
}

void AudioManager::shutdown()
{
    stopMusic();
    _track.reset();
    _music.clear();
    _activeMusic = AudioTrack::Total;
    _mixer.reset();
    if (_mixInitialized)
    {
        MIX_Quit();
        _mixInitialized = false;
    }
}

void AudioManager::toggleMusic() const
{
    if (!_track)
    {
        return;
    }

    if (MIX_TrackPlaying(_track.get()))
    {
        stopMusic();
    }
    else
    {
        playMusic(_activeMusic == AudioTrack::Total ? AudioTrack::BGM : _activeMusic);
    }
}

void AudioManager::playMusic(AudioTrack music) const
{
    if (!_track)
    {
        return;
    }
    auto selected = _music.find(music);
    if (selected == _music.end())
    {
        const auto& audio = Config::getInstance().getConfigData().audio;
        auto source = audio.find(toString(music));
        if (music == AudioTrack::BGM)
        {
            const auto legacy = audio.find("background_music");
            if (legacy != audio.end())
            {
                source = legacy;
            }
        }
        if (source == audio.end() || source->second.empty())
        {
            return;
        }
        std::unique_ptr<MIX_Audio, decltype(&MIX_DestroyAudio)> loaded(
            MIX_LoadAudio(_mixer.get(), source->second.c_str(), false), MIX_DestroyAudio);
        if (!loaded)
        {
            std::cerr << "Audio Manager Load Error for '" << source->second << "': " << SDL_GetError() << '\n';
            return;
        }
        selected = _music.emplace(music, std::move(loaded)).first;
    }
    if (_activeMusic != music)
    {
        stopMusic();
        if (!MIX_SetTrackAudio(_track.get(), selected->second.get()))
        {
            std::cerr << "Audio Manager Track Error: " << SDL_GetError() << '\n';
            return;
        }
        _activeMusic = music;
    }
    if (_track && !MIX_TrackPlaying(_track.get()))
    {
        SDL_PropertiesID properties = SDL_CreateProperties();
        if (!properties)
        {
            std::cerr << "Audio Manager Properties Error: " << SDL_GetError() << '\n';
            return;
        }
        SDL_SetNumberProperty(properties, MIX_PROP_PLAY_LOOPS_NUMBER, -1);
        if (!MIX_PlayTrack(_track.get(), properties))
        {
            std::cerr << "Audio Manager Playback Error: " << SDL_GetError() << '\n';
        }
        SDL_DestroyProperties(properties);
    }
}

void AudioManager::stopMusic() const
{
    if (_track)
    {
        MIX_StopTrack(_track.get(), 0);
    }
}

void AudioManager::resumeMusic() const
{
    if (_track && MIX_TrackPaused(_track.get()))
    {
        MIX_ResumeTrack(_track.get());
    }
    else
    {
        playMusic(_activeMusic == AudioTrack::Total ? AudioTrack::BGM : _activeMusic);
    }
}

void AudioManager::pauseMusic() const
{
    if (_track)
    {
        MIX_PauseTrack(_track.get());
    }
}
