# Biometric Authentication

This guide explains how to set up and use biometric authentication for your identity key in Grotto Android.

## Overview

Biometric authentication adds an extra layer of security by requiring fingerprint or face recognition before accessing your cryptographic identity key. This ensures that even if someone has physical access to your device, they cannot decrypt your messages without your biometric credential.

## Features

- **Fingerprint authentication** - Use your device's fingerprint sensor
- **Face recognition** - Use face unlock (on supported devices)
- **Hardware-backed security** - Biometric data is stored in secure hardware
- **Optional** - Can be enabled or disabled at any time
- **Fallback** - Passphrase is always available as fallback

## Requirements

- Android 6.0 (API 23) or higher
- Device with fingerprint sensor or face recognition
- At least one biometric credential enrolled in device settings

## Setup

### 1. Check Device Compatibility

The app automatically checks if your device supports biometric authentication:

```kotlin
val manager = BiometricAuthManager(context)
when (manager.canAuthenticate()) {
    BiometricAvailability.Available -> {
        // Biometrics can be used
    }
    BiometricAvailability.NotEnrolled -> {
        // User needs to enroll biometrics in settings
    }
    BiometricAvailability.NoHardware -> {
        // Device doesn't support biometrics
    }
    // ... other cases
}
```

### 2. Enable Biometric Authentication

Users can enable biometric authentication in Settings:

1. Open Grotto
2. Go to Settings → Security → Biometric Authentication
3. Toggle "Unlock with Biometrics"
4. Authenticate with your fingerprint/face to confirm

### 3. Using Biometric Authentication

Once enabled, the app will prompt for biometric authentication when:

- Initializing the crypto engine (first app launch)
- Decrypting messages
- Signing authentication challenges
- Exporting identity keys

## Implementation

### Basic Authentication

```kotlin
val biometricManager = BiometricAuthManager(context)

// Check if biometrics is available
if (biometricManager.isStrongBiometricAvailable()) {
    // Authenticate
    val success = biometricManager.authenticate(
        activity = this,
        title = "Authenticate",
        subtitle = "Access your identity key"
    )
    
    if (success) {
        // Proceed with crypto operation
    }
}
```

### Crypto Operations with Biometrics

```kotlin
val cryptoManager = BiometricCryptoManager(biometricManager)

// Decrypt a message with biometric protection
val plaintext = cryptoManager.decryptWithBiometric(
    activity = this,
    senderId = "alice",
    recipientId = "bob",
    ciphertext = encryptedData,
    type = 1
)

// Initialize crypto with biometrics
cryptoManager.initializeWithBiometric(
    activity = this,
    store = nativeStore,
    userId = "myuser",
    passphrase = "mypassword"
)
```

### Composable UI

```kotlin
@Composable
fun MyScreen() {
    var showPrompt by remember { mutableStateOf(false) }
    
    Button(onClick = { showPrompt = true }) {
        Text("Decrypt Message")
    }
    
    if (showPrompt) {
        BiometricPrompt(
            title = "Decrypt Message",
            subtitle = "Authenticate to decrypt",
            onSuccess = {
                // Perform decryption
                showPrompt = false
            },
            onError = { error ->
                // Show error
                showPrompt = false
            },
            onDismiss = {
                showPrompt = false
            }
        )
    }
}
```

## Security Considerations

### Where Biometric Data is Stored

- **NOT in the app** - Biometric data never leaves the Android Keystore
- **Hardware-backed** - Stored in Trusted Execution Environment (TEE) or Secure Element (SE)
- **Not accessible** - App cannot read raw biometric data, only authentication results

### What Happens When Authentication Succeeds

1. Biometric hardware verifies the user's identity
2. Android Keystore releases a cryptographic key
3. The key is used to decrypt the identity key
4. Identity key is used for Signal Protocol operations

### Fallback to Passphrase

If biometric authentication fails or is unavailable:
- User can always use their passphrase
- Passphrase decrypts the identity key directly
- This ensures you never lose access to your messages

### Security Levels

Grotto requires **Strong biometric** (Class 3) authentication:
- Fingerprint sensors with spoof detection
- 3D face recognition
- Not basic face recognition (2D photos)

## Troubleshooting

### "Biometrics not available"

**Cause**: Device doesn't have biometric hardware or none enrolled.

**Solution**:
1. Check if your device has a fingerprint sensor or face recognition
2. Enroll a fingerprint/face in device Settings → Security
3. Try again

### "Biometric prompt doesn't appear"

**Cause**: Activity is not a FragmentActivity or context is wrong.

**Solution**: Ensure you're passing a `FragmentActivity` (not just `Activity` or `Context`).

### "Authentication fails repeatedly"

**Cause**: Dirty sensor, wet fingers, or changed appearance.

**Solution**:
1. Clean the fingerprint sensor
2. Dry your finger
3. Use fallback passphrase
4. Re-enroll biometrics in device settings if needed

### "App crashes when showing prompt"

**Cause**: Missing `androidx.biometric` dependency or wrong theme.

**Solution**: Ensure the dependency is in `build.gradle`:
```gradle
implementation "androidx.biometric:biometric:1.2.0-alpha05"
```

## Disabling Biometric Authentication

To disable:
1. Go to Settings → Security → Biometric Authentication
2. Toggle "Unlock with Biometrics" off
3. Authenticate one last time to confirm

Your identity key remains secure, encrypted with your passphrase.

## Best Practices

### For Users

1. **Enable biometrics** - Adds significant security with minimal inconvenience
2. **Have fallback ready** - Remember your passphrase in case biometrics fail
3. **Keep device updated** - Security patches improve biometric protection
4. **Don't share biometrics** - Unlike passwords, you can't change your fingerprint

### For Developers

1. **Always provide fallback** - Passphrase should always work
2. **Handle errors gracefully** - Biometrics can fail for many reasons
3. **Reset auth state** - Require re-authentication after sensitive operations
4. **Test on real devices** - Emulators don't always support biometrics properly

## Privacy

Grotto takes privacy seriously:

- **No biometric data collection** - We never see or store your fingerprint/face data
- **Local only** - All biometric processing happens on your device
- **No network transmission** - Biometric data never leaves the device
- **Standard Android APIs** - Uses official Android BiometricPrompt API

## References

- [Android Biometric Documentation](https://developer.android.com/training/sign-in/biometric-auth)
- [BiometricPrompt API](https://developer.android.com/reference/androidx/biometric/BiometricPrompt)
- [Android Keystore System](https://developer.android.com/training/articles/keystore)
