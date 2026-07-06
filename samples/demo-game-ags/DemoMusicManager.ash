// =============================================================================
//  DemoMusicManager.ash
//  Music Manager for the demo game
// =============================================================================



// Settings
#define MIDI_OUT_MT32   "4"
#define MIDI_OUT_GMD    "2"

// Path definitions
#define IMS_PACK        "$DATA$/iMWrapData/demo.ims"
#define SF2_BANK        "$DATA$/iMWrapData/SGM-V2.01.sf2"

// Sound definitions
#define MUSIC_BASEROOM  90
#define MUSIC_HOLODECK  91

#define SFX_DOOR_OPEN   100
#define SFX_DOOR_CLOSE  101
#define SFX_DOOR_UNLOCK 102
#define SFX_DOOR_LOCK   103

// Jump Hook definitions

#define JH_BASE_TO_HOLO 1
#define JH_HOLO_TO_BASE 2

// Routines exports
import void music_init();