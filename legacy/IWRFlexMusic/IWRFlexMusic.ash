// =============================================================================
//  IWRFlexMusic.ash  —  Module header
//  Multi-Sequence interactive music manager for AGS + agsimwrap
// =============================================================================

managed struct IWRFlexMusic {

    /// Arms a synchronized transition.
    ///
    /// The engine will wait for the Hook Jump SysEx matching [hookValue] 
    /// in [currentSoundId] and jump to the transition track.
    /// When that transition track finishes and triggers Marker 64 (0x40 SysEx),
    /// [targetSoundId] will automatically start on its Track 0.
    ///
    /// @param currentSoundId  The ID of the currently playing sequence.
    /// @param targetSoundId   The ID of the sequence to start after the transition.
    /// @param hookValue       The CMD value of the 0x30 Hook Jump SysEx.
    import static void SetTransition(int currentSoundId, int targetSoundId, int hookValue);
    
    /// Directly starts a sequence, bypassing any transition.
    import static void Play(int soundId);
    
    /// Stops a sequence.
    import static void Stop(int soundId);
    
    /// Bridge to Global Script Trigger
    import static function OnTrigger(int soundId, int markerId);
};
