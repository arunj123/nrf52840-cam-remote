# Gesture Engine Logic

This diagram illustrates the state transitions and timing logic for the `GestureEngine` (main button handling).

```mermaid
stateDiagram-v2
    [*] --> Idle
    
    state Idle {
        [*] --> WaitingForInterrupt
    }
    
    Idle --> Debouncing : on_main_button_wake()
    
    state Debouncing {
        [*] --> CheckHigh
        CheckHigh --> sleep_60ms
        sleep_60ms --> VerifyHigh
    }
    
    Debouncing --> Idle : Button Released (Glitch)
    
    Debouncing --> TriggerClick : Button Still Held
    
    state TriggerClick {
        [*] --> SendVolumeUp
        SendVolumeUp --> LedAndHid_100ms
        LedAndHid_100ms --> SendRelease
    }
    
    TriggerClick --> LongPressPolling
    
    state LongPressPolling {
        [*] --> PollLoop
        PollLoop --> sleep_20ms
        sleep_20ms --> CheckDuration
        CheckDuration --> PollLoop : < 800ms
    }
    
    LongPressPolling --> Idle : Released < 800ms
    
    LongPressPolling --> TriggerBurst : Held >= 800ms
    
    state TriggerBurst {
        [*] --> BeepLong
        BeepLong --> SendVolumeUp_Burst
        SendVolumeUp_Burst --> sleep_2000ms
        sleep_2000ms --> SendRelease_Burst
    }
    
    TriggerBurst --> WaitingForFinalRelease
    
    state WaitingForFinalRelease {
        [*] --> FinalPoll
        FinalPoll --> sleep_20ms
        sleep_20ms --> FinalPoll : Still Held
    }
    
    WaitingForFinalRelease --> Idle : Final Release
```

## Timing Constants
| Parameter | Value | Description |
| :--- | :--- | :--- |
| `kDebounceMs` | 60ms | Initial noise filtering |
| `kLongPressMs` | 800ms | Threshold for Burst mode |
| `kBurstHoldMs` | 2000ms | Duration of the Volume Up hold in Burst mode |
| `kPollMs` | 20ms | Interval for checking button state during hold |
