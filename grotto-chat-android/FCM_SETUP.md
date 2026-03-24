# Firebase Cloud Messaging Setup

This document describes how to set up Firebase Cloud Messaging (FCM) for Grotto Android.

## Prerequisites

1. A Firebase project (create at https://console.firebase.google.com/)
2. Your Android app added to the Firebase project
3. `google-services.json` configuration file downloaded

## Setup Steps

### 1. Create Firebase Project

1. Go to https://console.firebase.google.com/
2. Click "Create project" and follow the setup wizard
3. Once created, click "Add app" and select Android
4. Enter your package name: `fi.grotto.android`
5. Register the app and download `google-services.json`

### 2. Add Configuration File

Place the downloaded `google-services.json` file in:

```
grotto-android/app/google-services.json
```

**Note:** This file contains API keys and should NOT be committed to version control.
Add it to `.gitignore`:

```
app/google-services.json
```

### 3. Build the Project

The Firebase plugin is already configured in `build.gradle.kts`. Just sync and build:

```bash
cd grotto-android
./gradlew :app:assembleDebug
```

### 4. Test FCM

1. Run the app on a device or emulator
2. Check logcat for token registration:
   ```
   I/FcmRepository: FCM token registered successfully
   ```
3. Send a test message from Firebase Console:
   - Go to Firebase Console → Cloud Messaging
   - Click "Send your first message"
   - Target your app

## Architecture

### Privacy-First Design

Grotto uses a **privacy-preserving** FCM implementation:

- **No message content** in push notifications
- Only "wakeup" signals are sent via FCM
- Actual message content is fetched from the server when app wakes up
- This maintains end-to-end encryption

### Notification Types

| Type | Description | Payload |
|------|-------------|---------|
| `wakeup` | New messages available | `channel_id`, `sender_id`, `has_mention` |
| `message` | Message preview | `sender_name`, `channel_name`, `preview` (no content) |
| `call` | Incoming voice call | `caller_id`, `caller_name` |
| `channel_invite` | Invited to channel | `channel_id`, `channel_name`, `inviter_name` |

### Data Flow

```
Server detects offline user with pending messages
    ↓
Server sends FCM "wakeup" via Firebase API
    ↓
FCM delivers to device
    ↓
GrottoMessagingService receives message
    ↓
If has_mention → Show notification
    ↓
App connects to server and fetches messages
    ↓
Messages decrypted with Signal Protocol
```

## Server-Side Integration

The Grotto server needs to implement FCM token registration and push sending:

### Token Registration

When client sends `MT_FCM_TOKEN`:

```protobuf
message FcmTokenRegistration {
  string token = 1;        // FCM registration token
  string platform = 2;     // "android"
  string device_name = 3;  // Optional
}
```

Store the token associated with the user in the database.

### Sending Push Notifications

When a message is sent to an offline user:

1. Check if user has registered FCM tokens
2. Send HTTP POST to Firebase API:

```bash
curl -X POST https://fcm.googleapis.com/v1/projects/YOUR_PROJECT_ID/messages:send \
  -H "Authorization: Bearer YOUR_ACCESS_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "message": {
      "token": "DEVICE_FCM_TOKEN",
      "data": {
        "type": "wakeup",
        "channel_id": "general",
        "sender_id": "alice",
        "has_mention": "true"
      }
    }
  }'
```

## Troubleshooting

### Token not registering

- Check that `google-services.json` is in the correct location
- Verify package name matches Firebase configuration
- Check logcat for errors: `adb logcat | grep FcmRepository`

### Notifications not showing

- Android 13+: Ensure POST_NOTIFICATIONS permission is granted
- Check notification channels are created (Android 8+)
- Verify FCM token is registered with server

### Build errors

- Ensure Google Services plugin is applied in `build.gradle.kts`
- Check that Firebase BOM version is compatible with other dependencies

## Security Considerations

1. **Never send encrypted message content** via FCM
2. **Rotate FCM server keys** periodically
3. **Validate tokens** before storing (check format)
4. **Remove tokens** on logout or app uninstall

## References

- [Firebase Cloud Messaging Documentation](https://firebase.google.com/docs/cloud-messaging)
- [Android Notifications Guide](https://developer.android.com/develop/ui/views/notifications)
- [FCM HTTP Protocol](https://firebase.google.com/docs/cloud-messaging/http-server-ref)
