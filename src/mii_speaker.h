/*
 * mii_speaker.h
 *
 * Copyright (C) 2023 Michel Pollet <buserror@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once
#include <stdint.h>

#define MII_SPEAKER_FREQ		(44100)
#define MII_SPEAKER_FRAME_COUNT	4

struct mii_t;
struct snd_pcm_t;
typedef int16_t mii_audio_sample_t;

typedef struct mii_audio_frame_t {
	__uint128_t	start;
	uint16_t	fill;
	mii_audio_sample_t *	audio;
} mii_audio_frame_t;

typedef struct mii_speaker_t {
	struct mii_t * mii;
	void * 		alsa_pcm;		// alsa pcm handle
	bool		muted;			// if true, don't play anything
	float 		volume;			// volume, 0.0 to 10.0
	float 		vol_multiplier;	// sample multiplier, 0.0 to 1.0
	float 		cpu_speed;		// CPU speed in MHz, to calculate clk_per_sample
	uint8_t 	fplay;			// frame we want to play
	uint8_t		fplaying;		// index of the current playing frame
	uint16_t 	fsize;			// size in samples of a frame
	uint8_t		findex; 		// frame we are currently filling
	uint16_t 	clk_per_sample;	// number of cycles per sample (at current CPU speed)
	mii_audio_sample_t 	sample;	// current value for the speaker output
	mii_audio_frame_t	frame[MII_SPEAKER_FRAME_COUNT];
} mii_speaker_t;

// Initialize the speaker with the frame size in samples
void
mii_speaker_init(
		struct mii_t * mii,
		mii_speaker_t *speaker);
// Called when $c030 is touched, place a sample at the 'appropriate' time
void
mii_speaker_click(
		mii_speaker_t *speaker);
// Check to see if there's a new frame to send, send it
void
mii_speaker_run(
		mii_speaker_t *speaker);
// volume from 0 to 10, sets the audio sample multiplier.
void
mii_speaker_volume(
		mii_speaker_t *s,
		float volume);
