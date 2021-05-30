
#pragma once

#include "obj.h"

/*
	High level sound and music wrappers around tig_sound.
*/
class Sound {
	friend class SoundHooks;
	friend class SoundGameSystem;
public:

	/*
		Plays a sound with the given sound id (from the mes files).
		Returns the tig_sound stream id.
	*/
	int PlaySound(int soundId, int loopCount = 1);

	/*
		Plays a sound at the location of the given object.
		Otherwise works as PlaySound
	*/
	int PlaySoundAtObj(int soundId, objHndl obj, int loopCount = 1);
	
	/*
		Plays a sound at the given tile.
		Otherwise works as PlaySound
	*/
	int PlaySoundAtLoc(int soundId, locXY loc, int loopCount = 1);

	/*
	low level sound player?
	*/
	int MssPlaySound(int soundId);

private:
	void Init();
	static std::map<int, std::string> mUserSounds;
	MesHandle tpSounds = -1;
};

extern Sound sound;
