# Gesture Engine Logic

This diagram illustrates the state transitions and timing logic for the `GestureEngine` (main button handling).

## Logic Diagram
```mermaid
stateDiagram-v2
    direction LR

    %% Styles
    classDef idle fill:#2d3436,stroke:#0984e3,stroke-width:2px,color:#fff
    classDef proc fill:#2d3436,stroke:#fdcb6e,stroke-width:2px,color:#fff
    classDef act fill:#2d3436,stroke:#00b894,stroke-width:2px,color:#fff
    classDef wait fill:#2d3436,stroke:#d63031,stroke-width:2px,color:#fff

    [*] --> Idle
    Idle --> S_Debounce : onWake
    
    state "Debouncing" as S_Debounce {
        direction LR
        [*] --> Check
        Check --> Sleep60
        Sleep60 --> Verify
    }
    
    S_Debounce --> Idle : Glitch
    S_Debounce --> S_Click : Held
    
    state "Single Click" as S_Click {
        direction LR
        [*] --> HID_Up
        HID_Up --> Delay
        Delay --> HID_Rel
    }
    
    S_Click --> S_Poll : Entry
    
    state "Long Press Detection" as S_Poll {
        direction LR
        [*] --> Sleep20
        Sleep20 --> CheckT
        CheckT --> Sleep20 : <800ms
    }
    
    S_Poll --> Idle : Released
    S_Poll --> S_Burst : HeldLong
    
    state "Burst Mode" as S_Burst {
        direction LR
        [*] --> Beep
        Beep --> BurstUp
        BurstUp --> Sleep2s
        Sleep2s --> BurstRel
    }
    
    S_Burst --> S_WaitRel
    
    state "Wait for Release" as S_WaitRel {
        direction LR
        [*] --> PollRel
        PollRel --> Sleep20_F
        Sleep20_F --> PollRel : StillHeld
    }
    
    S_WaitRel --> Idle : Released

    class Idle idle
    class S_Debounce proc
    class S_Click act
    class S_Poll proc
    class S_Burst act
    class S_WaitRel wait
```

## Timing Constants
| Parameter | Value | Description |
| :--- | :--- | :--- |
| `kDebounceMs` | 60ms | Initial noise filtering |
| `kLongPressMs` | 800ms | Threshold for Burst mode |
| `kBurstHoldMs` | 2000ms | Duration of the Volume Up hold in Burst mode |
| `kPollMs` | 20ms | Interval for checking button state during hold |
