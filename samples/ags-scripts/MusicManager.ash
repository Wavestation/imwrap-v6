// =============================================================================
//  MusicManager.ash  —  Module header  (v2 — Generic / Drop-in)
//  States and Sequences system for AGS + agsimwrap plugin
//
//  PHILOSOPHY: 100% REUSABLE MODULE
//  ----------------------------------
//  This file contains NO project-specific data.
//  All configuration is injected from the game's script via:
//
//    MusicManager.Init(soundId, transitionEndMarkerId)
//    MusicManager.RegisterState(stateId, trackId, entryBeat, entryTick)
//    MusicManager.RegisterSequence(seqId, trackId, startBeat, startTick, hookValue)
//
//  Music loops remain in the .ims file, managed by the iMWrap engine when it
//  reads the 0x50 SysEx as the playhead passes through a section.
//
//  QUICK START
//  -----------
//  In game_start():
//
//    iMWrap_LoadBank("music/game.ims");
//    iMWrap_StartSound(MY_SOUND_ID);
//    MusicManager.Init(MY_SOUND_ID, MY_END_MARKER_ID);
//    MusicManager.RegisterState(STATE_MENU,        0,  1, 0);
//    MusicManager.RegisterState(STATE_EXPLORATION, 0,  9, 0);
//    MusicManager.RegisterSequence(SEQ_MENU_TO_EXPL, 0, 65, 0, 1);
//    // ...
//    MusicManager.SetState(STATE_MENU);
//
//  In-game:
//    MusicManager.SetState(STATE_COMBAT);
//    MusicManager.Transition(SEQ_COMBAT_TO_VICTORY, STATE_VICTORY);
//
//  EXTENDING THE MODULE
//  --------------------
//  - Add values to the enums below (they are just integer labels).
//  - Call RegisterState / RegisterSequence in game_start().
//  - Never modify MusicManager.asc to add project-specific data.
// =============================================================================

// ---------------------------------------------------------------------------
//  MODULE CAPACITY (internal table sizes)
//  Increase if your project needs more states or sequences.
// ---------------------------------------------------------------------------

/// Maximum number of states that can be registered.
#define MUSIC_MAX_STATES      32

/// Maximum number of transition sequences that can be registered.
#define MUSIC_MAX_SEQUENCES   32

// ---------------------------------------------------------------------------
//  ENUM: Music States  (template — rename and extend for your project)
//
//  These values are plain integers used as indices in the module's internal
//  tables. Extend or rename freely.
//  STATE_NONE (-1) is reserved and must not be registered.
// ---------------------------------------------------------------------------
enum MusicStateId {
    STATE_NONE        = -1,   ///< Reserved — no active state
    STATE_MENU        =  0,
    STATE_EXPLORATION =  1,
    STATE_TENSION     =  2,
    STATE_COMBAT      =  3,
    STATE_VICTORY     =  4,
    STATE_SAD         =  5,
    // -- Add your own states here --
};

// ---------------------------------------------------------------------------
//  ENUM: Transition Sequences  (template — rename and extend for your project)
//
//  Each value identifies a musical bridge leading from one state to another.
//  SEQ_NONE (-1) is reserved.
// ---------------------------------------------------------------------------
enum MusicSequenceId {
    SEQ_NONE                   = -1,   ///< Reserved — no sequence
    SEQ_MENU_TO_EXPLORATION    =  0,
    SEQ_EXPLORATION_TO_TENSION =  1,
    SEQ_TENSION_TO_COMBAT      =  2,
    SEQ_COMBAT_TO_VICTORY      =  3,
    SEQ_COMBAT_TO_EXPLORATION  =  4,
    SEQ_ANY_TO_SAD             =  5,
    // -- Add your own sequences here --
};

// ---------------------------------------------------------------------------
//  INTERNAL STRUCTS
//  Not intended to be instantiated directly by the game designer.
// ---------------------------------------------------------------------------

/// Entry point of a music state.
/// The loop is defined in the .ims via SysEx 0x50.
struct MusicStateData {
    int  trackId;         ///< MIDI track of the section (0 for single-track layouts)
    int  entryBeat;       ///< Target measure for iMWrap_Jump()
    int  entryTick;       ///< Start tick (usually 0)
    bool registered;      ///< true if this state was registered via RegisterState()
};

/// Definition of a transition sequence.
struct MusicSequenceData {
    int  trackId;         ///< MIDI track of the bridge.
                          ///<   Hook mode  (hookValue > 0): unused by the script —
                          ///<   the 0x30 SysEx in the MIDI encodes the target track.
                          ///<   Direct mode (hookValue == 0): used by iMWrap_Jump().
    int  startBeat;       ///< Start measure of the bridge
    int  startTick;       ///< Start tick (usually 0)
    int  hookValue;       ///< CMD value of the 0x30 Hook Jump SysEx armed by iMWrap_SetHook.
                          ///< Must match the [CMD] field of the SysEx in the MIDI:
                          ///<   F0 7D 30 00 [hookValue] [TARGET_TRACK] [BEAT] [TICK] F7
                          ///< Set to 0 to use direct-jump mode (iMWrap_Jump) instead.
    bool registered;      ///< true if this sequence was registered via RegisterSequence()
};

// ---------------------------------------------------------------------------
//  MAIN STRUCT: MusicManager
// ---------------------------------------------------------------------------

/// Interactive music manager — States and Sequences (iMUSE-style).
managed struct MusicManager {

    // -----------------------------------------------------------------------
    //  INITIALIZATION
    // -----------------------------------------------------------------------

    /// Initializes the module. Call once in game_start() AFTER
    /// iMWrap_LoadBank() and iMWrap_StartSound().
    ///
    /// @param soundId              ID of the sound in the .ims bank to control.
    /// @param transitionEndMarkerId  Value of the MIDI marker (SysEx 0x40) placed
    ///                             at the end of every transition bridge.
    ///                             Must match the byte in the MIDI:
    ///                               F0 7D 40 00 [transitionEndMarkerId] F7
    import static void Init(int soundId, int transitionEndMarkerId);

    // -----------------------------------------------------------------------
    //  DATA REGISTRATION  (call from game_start)
    // -----------------------------------------------------------------------

    /// Registers a music state.
    /// The loop for this state is defined in the .ims (SysEx 0x50);
    /// the module only needs to know the entry point.
    ///
    /// @param stateId    MusicStateId enum value (integer >= 0).
    /// @param trackId    MIDI track of the section.
    ///                   Use 0 if all states share the same main track.
    ///                   Use distinct values if each state has its own track.
    /// @param entryBeat  Start measure of the section in the MIDI.
    /// @param entryTick  Start tick (0 in most cases).
    import static void RegisterState(int stateId, int trackId, int entryBeat, int entryTick);

    /// Registers a transition sequence.
    ///
    /// @param seqId      MusicSequenceId enum value (integer >= 0).
    /// @param trackId    MIDI track of the bridge.
    ///                   Hook mode  (hookValue > 0): not used by the script —
    ///                   the 0x30 SysEx in the MIDI encodes the target track.
    ///                   Pass the correct value anyway for documentary consistency.
    ///                   Direct mode (hookValue == 0): the script uses this trackId
    ///                   to call iMWrap_Jump() directly toward the bridge.
    /// @param startBeat  Start measure of the bridge.
    /// @param startTick  Start tick (0 in most cases).
    /// @param hookValue  CMD value of the 0x30 Hook Jump SysEx for a synchronized
    ///                   jump (arms iMWrap_SetHook). Must be unique per sequence.
    ///                   Pass 0 for an immediate direct jump (iMWrap_Jump) without a hook.
    import static void RegisterSequence(int seqId, int trackId, int startBeat, int startTick, int hookValue);

    // -----------------------------------------------------------------------
    //  IN-GAME CONTROL
    // -----------------------------------------------------------------------

    /// Switches immediately to a registered state.
    /// Clears the previous loop, jumps to entryBeat, and lets the engine
    /// read the 0x50 SysEx from the MIDI to arm the new loop automatically.
    ///
    /// @param stateId  MusicStateId enum value.
    import static void SetState(int stateId);

    /// Returns the ID of the currently active state (or STATE_NONE if none).
    import static int GetCurrentState();

    /// Triggers a synchronized transition to a new state.
    /// Clears the loop, arms the iMUSE Hook (iMWrap_SetHook), and waits for
    /// the engine to execute the Hook Jump SysEx in the MIDI to jump to the
    /// bridge. When the bridge ends (Marker 0x40), SetState(targetStateId)
    /// is called automatically.
    ///
    /// @param seqId         MusicSequenceId enum value.
    /// @param targetStateId Destination state after the bridge.
    import static void Transition(int seqId, int targetStateId);

    /// Returns true if a transition is currently in progress.
    import static bool IsTransitionPending();

    /// Silences the music and resets the module to its initial state.
    import static void Stop();

};
