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
    Idle --> DB : onWake
    
    state "Debouncing" as DB {
        direction LR
        [*] --> Check
        Check --> Sleep60
        Sleep60 --> Verify
    }
    
    DB --> Idle : Glitch
    DB --> Click : Held
    
    state "Single Click" as Click {
        direction LR
        [*] --> HID_Up
        HID_Up --> Delay
        Delay --> HID_Rel
    }
    
    Click --> Poll : Entry
    
    state "Long Press Detection" as Poll {
        direction LR
        [*] --> Sleep20
        Sleep20 --> CheckT
        CheckT --> Sleep20 : <800ms
    }
    
    Poll --> Idle : Released
    Poll --> Burst : Held >= 800ms
    
    state "Burst Mode" as Burst {
        direction LR
        [*] --> Beep
        Beep --> BurstUp
        BurstUp --> Sleep2s
        Sleep2s --> BurstRel
    }
    
    Burst --> WaitRel
    
    state "Wait for Release" as WaitRel {
        direction LR
        [*] --> PollRel
        PollRel --> Sleep20_F
        Sleep20_F --> PollRel : StillHeld
    }
    
    WaitRel --> Idle : Released

    class Idle idle
    class DB proc
    class Click act
    class Poll proc
    class Burst act
    class WaitRel wait
```

## Timing Constants
| Parameter | Value | Description |
| :--- | :--- | :--- |
| `kDebounceMs` | 60ms | Initial noise filtering |
| `kLongPressMs` | 800ms | Threshold for Burst mode |
| `kBurstHoldMs` | 2000ms | Duration of the Volume Up hold in Burst mode |
| `kPollMs` | 20ms | Interval for checking button state during hold |
