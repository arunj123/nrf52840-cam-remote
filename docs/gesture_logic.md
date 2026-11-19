# Gesture Engine Logic

This diagram illustrates the state transitions and timing logic implemented in `GestureEngine` (`firmware/src/gesture_engine.hpp`) for the main shutter buttons, rotary encoder, and encoder push button.

## Main Button Logic
```mermaid
stateDiagram-v2
    direction TB

    %% Styles
    classDef idle fill:#2d3436,stroke:#0984e3,stroke-width:2px,color:#fff
    classDef proc fill:#2d3436,stroke:#fdcb6e,stroke-width:2px,color:#fff
    classDef act fill:#2d3436,stroke:#00b894,stroke-width:2px,color:#fff
    classDef wait fill:#2d3436,stroke:#d63031,stroke-width:2px,color:#fff

    [*] --> Idle
    Idle --> S_Debounce : onMainButtonWake (Edge Active)

    state "Debouncing (60ms)" as S_Debounce
    S_Debounce --> Idle : Glitch (Released)
    S_Debounce --> S_HoldPoll : Still Held

    state "Hold Duration Poll (20ms)" as S_HoldPoll {
        direction LR
        [*] --> Poll
        Poll --> Poll : "< 5000ms (sets burst=true at >= 800ms)"
    }

    S_HoldPoll --> S_ClearBonds : Held >= 5000ms
    S_HoldPoll --> S_Burst : Released (800ms .. 4999ms)
    S_HoldPoll --> S_Click : Released (< 800ms)

    state "Clear Bonds & Rotate ID" as S_ClearBonds {
        direction LR
        [*] --> UnpairAndCountdownBeep
    }
    S_ClearBonds --> Idle

    state "Burst Mode (mapped main_button_long)" as S_Burst {
        direction LR
        [*] --> LongBeepLedOn
        LongBeepLedOn --> HoldKey2s
        HoldKey2s --> ReleaseKeyLedOff
    }
    S_Burst --> S_WaitRel : Wait until released
    S_WaitRel --> Idle

    state "Single Click (mapped main_button_single)" as S_Click {
        direction LR
        [*] --> LedOnBeep
        LedOnBeep --> SendKey100ms
        SendKey100ms --> ReleaseLedOffDebounce
    }
    S_Click --> Idle

    class Idle idle
    class S_Debounce proc
    class S_HoldPoll proc
    class S_Click act
    class S_Burst act
    class S_ClearBonds wait
    class S_WaitRel wait
```

## Encoder Rotation & Button Logic
```mermaid
stateDiagram-v2
    direction TB

    classDef idle fill:#2d3436,stroke:#0984e3,stroke-width:2px,color:#fff
    classDef proc fill:#2d3436,stroke:#fdcb6e,stroke-width:2px,color:#fff
    classDef act fill:#2d3436,stroke:#00b894,stroke-width:2px,color:#fff

    [*] --> Idle
    Idle --> E_Debounce : onEncoderButtonPress (Edge Active)

    state "Debouncing (60ms)" as E_Debounce
    E_Debounce --> Idle : Glitch
    E_Debounce --> E_Poll : Held

    state "Long Press Detection (20ms)" as E_Poll {
        direction LR
        [*] --> PollHeld
        PollHeld --> Sleep20
        Sleep20 --> PollHeld : <800ms
    }

    E_Poll --> E_ShortAction : Released (<800ms, mapped encoder_button_short)
    E_Poll --> E_ProfileSwitch : HeldLong (>=800ms)

    state "Mapped Short Action (Default: Mute)" as E_ShortAction
    E_ShortAction --> Idle

    state "Profile Switch (0 -> 1 -> 2 -> 3 -> 0)" as E_ProfileSwitch
    E_ProfileSwitch --> E_WaitRel

    state "Wait for Release" as E_WaitRel
    E_WaitRel --> Idle : Released
```

- **Rotary Encoder (`on_encoder_rotate`)**: Deferred from the `qdec_nrfx` ISR to `encoder_rot_thread`. Accumulates degrees and emits up to `kMaxStepsPerReport` (`10`) HID press/release events per batch at `kDegreesPerDetent` (`60`°) intervals using `ConfigMappings.encoder_cw` (negative) or `ConfigMappings.encoder_ccw` (positive).

## Timing & Geometry Constants
| Parameter | Value | Description |
| :--- | :--- | :--- |
| `kDebounceMs` | 60 ms | Initial noise filtering & post-click cooldown |
| `kPollMs` | 20 ms | Interval for checking button state during hold |
| `kLongPressMs` | 800 ms | Threshold for Burst Mode / Profile Switch |
| `kBurstHoldMs` | 2000 ms | Duration the mapped key stays held in Burst Mode |
| `kResetHoldMs` | 5000 ms | Threshold to clear bonds and rotate slot BT ID |
| `kDegreesPerDetent` | 60 deg | QDEC angle threshold per encoder detent |
| `kMaxStepsPerReport` | 10 | Upper bound on detents emitted in a single rotation call |
