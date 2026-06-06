# MetaModule MIDI Testing Notes

## 1. Protocol Conversion

The system converts between two MIDI message representations:

### `rack::midi::Message` (VCV Rack / MetaModule)
- Fixed 3-byte structure with USB header (cable + CIN/code)
- USB MIDI uses CIN (Code Index Number) to indicate packet type and byte count
- Sysex is fragmented across multiple packets using CIN values 4, 5, 6, 7

### `EraeApi::MidiMsg`
- Variable-length message (any size)
- Complete sysex arrives as single message with F0...F7 boundaries
- Used by the EraeApi protocol layer internally

### Conversion: rack → Erae (Inbound)
USB MIDI packet types:
| CIN | Meaning | Valid Bytes |
|-----|---------|-------------|
| 4 | SysEx Start/Continue | 3 bytes |
| 5 | SysEx End (1 byte) | 1 byte |
| 6 | SysEx End (2 bytes) | 2 bytes |
| 7 | SysEx End (3 bytes) | 3 bytes |
| 0 | Regular MIDI (non-sysex) | 3 bytes |

The `MMMidiDevice::onMessage(rack::midi::Message)` reassembles fragmented USB MIDI packets into a single `EraeApi::MidiMsg`.

### Conversion: Erae → rack (Outbound)
A complete sysex message is broken into USB MIDI packets:
1. `startSysEx(b0, b1)` - CIN=4, sends F0 + first 2 data bytes
2. `continueSysEx(b0, b1, b2)` - CIN=4, sends 3 data bytes
3. `endSysEx()` / `endSysEx(b0)` / `endSysEx(b0, b1)` - CIN=5/6/7 for final packet

## 2. Issues Found

### Issue A: Inbound Sysex Incomplete
- **Symptom**: Debug shows `S4.....E18X` (18 bytes) instead of expected `S4.........E30X` (30 bytes)
- **Cause**: Packets being dropped at hardware/platform level before reaching `onMessage()`
- **Status**: Hardware issue - queue overflow or USB buffer dropping packets

### Issue B: Outbound Sysex Not Transmitted
- **Symptom**: `send()` is called and debug shows message formatted, but USB MIDI monitor sees no sysex
- **Cause**: Unknown hardware/driver issue
- **Status**: Hardware issue - MIDI notes work but sysex does not

### Issue C: Real-Time Messages Interrupting Sysex
- **Original**: Case 0 cleared sysex state for all messages including real-time (0xF8-0xFF)
- **Fix**: Case 0 now ignores/drops real-time messages during active sysex
- **Verification**: Debug showed `E21X` pattern indicating state was being cleared mid-stream

## 3. Debugging Included

### Debug String Markers (64 char rolling buffer)

**Inbound (receive) markers:**
- `S4` - Sysex start, CIN=4
- `S6` - Sysex start, CIN=6
- `S7` - Sysex start, CIN=7
- `.` (dot) - Continuation packet received during active sysex
- `c5` - Continuation in CIN=5 (sysExActive_=true)
- `c6` - Continuation in CIN=6 (sysExActive_=true)
- `c7` - Continuation in CIN=7 (sysExActive_=true)
- `E5`, `E6`, `E7` - End packet detected (with F7 in data)
- `X` - Message fully built and sent via `onMessage(EraeApi::MidiMsg)`
- `SE7` - Complete single-packet sysex (F0...F7 in one CIN=7)

**Outbound (send) markers:**
- `>` - send() called
- `>[4` - Multibyte sysex started with CIN=4 (startSysEx)
- `,` (comma) - Continuation packet sent
- `E5`, `E6`, `E7` - End packet type
- `>16.0` - Size and first data byte shown at end of multibyte send
- `>z` - Zero-length sysex sent (sysExNoPayload)
- `>N` - Single-byte sysex sent (sysExSingleByte N=byte value)

### Debug Flags
- `debugState_` - Set true when sysex start detected, set false when 32-byte complete message received
- `debugString_` - Rolling 64-char debug log of all MIDI events

## 4. Expected vs Actual Results

### Expected (on working hardware)

**Touch msg receive:**
```
S4.........E30X
```
- S4 (start CIN=4)
- 9 dots (9 continuation packets)
- E30X (end, 30 bytes accumulated, built and queued)

**Init messages send:**
```
>[4,,,E6>16.0>[4,E6>14.0>[4,E6>14.0
```
- Three init messages sent (16, 14, 14 bytes)

**Init response receive:**
```
S4.....E14X
```
- API version response from Erae

### Actual (on current hardware)

**Touch msg receive (partial):**
```
S4.....E18X
```
- Only 5-6 packets arrive (missing 3-4 packets at hardware level)
- Consistent pattern: E18, E15, E21, E18 - always missing multiples of 3 bytes

**Init messages send:**
```
>[4,,,E6>16.0>[4,E6>14.0>[4,E6>14.0
```
- All three messages formatted and send() called
- No sysex visible on USB MIDI monitor
- No response received

## 5. Next Steps

### Hardware Team
1. Investigate USB MIDI queue depth - messages may be overflowing
2. Check if real-time messages (MIDI Clock 0xF8) are being injected during sysex
3. Verify sysex packets are reaching USB bus at hardware level

### Software (when hardware is fixed)
1. Remove or consolidate debug code in `MMMidiDevice.cpp`
2. Add queue depth configuration if needed
3. Verify touch messages arrive complete (E30X)
4. Verify init sequence gets response from Erae

### Debug Code Location
- `src/MMMidiDevice.cpp` - `onMessage()`, `send()`, `buildSysExMsg()`, `queueInMsg()`
- `src/EraeTouch.cpp` - `processMidi()`, display update in `get_display_text()`

---

## Future Agent Prompt

When hardware team provides updates, use this prompt to continue debugging:

```
I need to help debug MIDI issues on MetaModule platform.
Current situation:
- The Erae touch module (src/EraeTouch.cpp) communicates via MMMidiDevice (src/MMMidiDevice.cpp)
- MMMidiDevice converts between rack::midi::Message (USB MIDI fragmented) and EraeApi::MidiMsg (complete messages)
- Two hardware issues were identified before escalation to hardware team:

1. INBOUND ISSUE: Touch messages (32 bytes) arrive incomplete. Debug shows E18/E21 instead of E30.
   - Packets are being dropped at hardware/platform level
   - Expected: S4.....E30X (full 30 data bytes)
   - Actual: S4.....E18X (missing 3-4 continuation packets)

2. OUTBOUND ISSUE: Sysex init messages are formatted and send() is called,
   but USB MIDI monitor shows no sysex reaching the device.
   - Debug shows messages like: >[4,,,E6>16.0
   - No sysex visible on hardware bus
   - MIDI notes work, only sysex fails

Recent changes made:
- Fixed case 0 to not clear sysex state on real-time messages (0xF8-0xFF)
- Fixed continuation loop in send(): changed condition from `offset + 3 < msg.size()`
  to `offset + 3 < msg.size() - 1` to prevent reading F7 as data
- Added debug instrumentation with 64-char rolling buffer
  (inbound uses: S4, S6, S7, dots, E5/E6/E7, X)
  (outbound uses: >, >[4, commas, E5/E6/E7)

Files to examine:
- src/MMMidiDevice.cpp - contains onMessage() for inbound reassembly, send() for outbound fragmentation
- src/EraeTouch.cpp - processMidi() passes MIDI to device, get_display_text() shows debugString
- rack-interface/include/midi.hpp - defines rack::midi::Message structure and USB MIDI encoding

The hardware team is investigating. Once they provide updates:
1. Run the module and observe debugString on the display
2. For inbound: verify E30X instead of E18/E21
3. For outbound: use USB MIDI monitor to check if sysex appears
4. Check for any new debug markers or unexpected patterns
```

---

## Hardware Team Response Template

When hardware team responds, update this section:

**Hardware Team Feedback:**
*[Date and findings from hardware team]*

**Test Plan:**
*[Steps to verify fix]*

**Expected Results After Fix:**
- Inbound: `S4.........E30X` (9 dots for 9 continuation packets)
- Outbound: Sysex visible on USB MIDI monitor, Erae responds with reply
- Complete handshake: Init messages sent → Response received → Touch data flows