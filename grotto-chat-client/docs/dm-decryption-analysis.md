# DM Decryption Failure Analysis (-1005)

**Error:** `-1005 = SG_ERR_INVALID_MESSAGE`

## Root Causes Identified

### 1. Stale Session State (Most Common)
When one client has an established outgoing session (stored in DB from previous run) 
but the other client doesn't have the matching incoming session state (e.g., fresh install 
or deleted DB), incoming WHISPER_MESSAGEs fail with "No valid sessions".

**Scenario:**
1. hittoLinux has old session state from previous run
2. hittoAEAE is fresh (no session state)
3. hittoLinux sends WHISPER_MESSAGE using old session
4. hittoAEAE cannot decrypt → -1005

### 2. SPK Regeneration on Restart
The Signed Pre-Key is regenerated on every client restart. Any offline messages or 
messages in flight encrypted with the OLD SPK cannot be decrypted after reconnect.

**This produces:** -1005 "mac not verified"

### 3. Pre-Key ID Reuse
`next_opk_id_` counter resets to 1 on each restart. If client generates new pre-keys 
with same IDs but different private keys, server has old public keys causing X3DH mismatch.

**Status:** ✅ Fixed by persisting counter to database

### 4. Race Condition: Key Upload Timing
When client A connects and uploads new SPK, there's a window where:
- Server might still have old SPK
- Client B requests key bundle → gets old SPK
- Client B encrypts with old SPK
- Meanwhile Client A now has new SPK locally
- Decryption fails because SPKs don't match

## Error Contexts in libsignal

From `session_cipher.c`, -1005 can mean:

| Context | Meaning |
|---------|---------|
| WHISPER_MESSAGE | "No valid sessions" - no matching session state found |
| WHISPER_MESSAGE | "Uninitialized session!" - sender_chain isn't present |
| WHISPER_MESSAGE | Version mismatch between message and session |
| PRE_KEY_MESSAGE | "Message mac not verified" - X3DH keys mismatch |

## Proposed Fixes

### Critical

1. **Prevent SPK regeneration on restart**
   - Load existing SPK from database
   - Only generate new SPK on first install

2. **Add recovery mechanism for failed decryption**
   ```
   When WHISPER_MESSAGE decryption fails:
   - Log detailed diagnostics (session exists? which pre_key_id?)
   - Delete stale session
   - Send implicit KEY_REQUEST by initiating outgoing session
   - This triggers sender to re-establish fresh session
   ```

### Medium Priority

3. **Queue messages that arrive before session ready**
   - Buffer incoming ciphertexts when no session exists
   - Process after session establishment

4. **Re-key request on failure**
   - Send explicit signal to sender when decryption fails
   - Allows sender to establish fresh session

## Logging Needed

Add diagnostics to pinpoint failure:
- Ciphertext type (3=PRE_KEY, 2=WHISPER)
- Pre-key ID used
- Session exists in store?
- SPK ID local vs message
- Raw error from libsignal at each step

## Session State Flow

```
Alice wants to send to Bob:
1. Check if session exists
   - Yes: Send WHISPER_MESSAGE
   - No: Request KEY_BUNDLE from server

2. Process KEY_BUNDLE:
   - Build X3DH session
   - Send PRE_KEY_MESSAGE

3. Bob receives PRE_KEY_MESSAGE:
   - Processes X3DH
   - Creates session state
   - Can now decrypt

4. Race condition problem:
   - Alice already had session (step 1: Yes)
   - Bob never received Alice's first message
   - Bob has no session
   - Alice sends WHISPER_MESSAGE
   - Bob cannot decrypt
```

## Recommended Immediate Fix

When decryption fails for WHISPER_MESSAGE:
1. Delete local session for that sender
2. Initiate outgoing session (send KEY_REQUEST)
3. This forces both sides to sync on fresh session

## References

- Signal Protocol X3DH: https://signal.org/docs/specifications/x3dh/
- libsignal-protocol-c: session_cipher.c, session_builder.c
