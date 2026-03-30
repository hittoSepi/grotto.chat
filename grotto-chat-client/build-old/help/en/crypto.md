# Encryption & Trust

In Grotto the server carries messages but never sees their content. Everything is end-to-end encrypted.

## How It Works

- Every user gets their own `Ed25519` identity key on first launch
- Private messages use the **Signal Protocol** (X3DH + Double Ratchet)
- Channel messages use **Sender Key** encryption for efficient group messaging

## Trusting Users

When someone sends messages, Grotto shows their public key. The first time it's "unknown".

Type `/trust <name>` to verify you're talking to the right person. If the key changes later, Grotto warns you.

## Key Management

Your identity key is stored locally and protected by your password. If you forget your password or want a clean slate:

1. Close Grotto
2. Restart with the `--clear-creds` flag
3. Log in with the same username and a new password
4. Server keys will re-sync
5. Remember to re-trust your contacts

---

Server sees: who talks to whom, when, and from what IP.  
Server DOES NOT SEE: message content, file content, voice call audio.
