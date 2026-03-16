# Certificate Pinning Setup Guide

This guide explains how to set up certificate pinning for Grotto Android to prevent MITM attacks.

## What is Certificate Pinning?

Certificate pinning ensures your app only connects to servers with specific SSL certificates. Even if a malicious actor compromises a Certificate Authority (CA), they cannot intercept your app's traffic because the app only trusts the pinned certificate.

## Getting the Certificate Pin

### From a Certificate File

If you have the server's certificate file (`server.crt`):

```bash
# Extract the public key and compute SHA-256 hash
openssl x509 -in server.crt -pubkey -noout | \
  openssl pkey -pubin -outform der | \
  openssl dgst -sha256 -binary | \
  openssl enc -base64

# Output example: rE2/zFR1L5z0U4Y8Jq/x8O3K8n9...
```

### From a Running Server

If the server is already running:

```bash
# Connect and extract the certificate
openssl s_client -connect your-server.com:6667 -servername your-server.com < /dev/null 2>/dev/null | \
  openssl x509 -pubkey -noout | \
  openssl pkey -pubin -outform der | \
  openssl dgst -sha256 -binary | \
  openssl enc -base64
```

### From Android Studio

When connecting to an unpinned server, the app will log the certificate pins:

```
W/CertificatePinner: Certificate pin mismatch for your-server.com
W/CertificatePinner: Expected one of: []
W/CertificatePinner: Got: [rE2/zFR1L5z0U4Y8Jq/x8O3K8n9...]
```

Copy the pin from the log and add it to the app.

## Adding a Certificate Pin

### Via UI

1. Open Grotto Android
2. Go to Settings → Security → Certificate Pinning
3. Tap the + button
4. Enter:
   - **Hostname pattern**: `*.your-server.com` or `your-server.com`
   - **SHA-256 Pin**: The base64-encoded hash (e.g., `rE2/zFR1L5z0U4Y8Jq/x8O3K8n9...`)
   - **Backup pin**: Check if this is a backup certificate
5. Tap "Add"

### Programmatically

```kotlin
val pinRepository: PinRepository

// Add a pin
pinRepository.addPin(
    hostname = "*.your-server.com",
    pin = "rE2/zFR1L5z0U4Y8Jq/x8O3K8n9...",
    isBackupPin = false
)
```

## Backup Pins (Certificate Rotation)

When you need to renew your server's certificate:

1. Generate a new certificate
2. Compute its pin
3. Add it as a **backup pin** before deploying
4. Deploy the new certificate
5. Remove the old pin after confirming the new one works

This ensures zero-downtime certificate rotation.

## Trust On First Use (TOFU)

For self-hosted servers with self-signed certificates, Grotto supports TOFU:

1. First connection: App sees unknown certificate
2. App asks user to verify the certificate fingerprint
3. If user accepts, the pin is saved automatically
4. Future connections use the saved pin

## Security Considerations

### Pin Expiration
- Certificate pins don't expire, but certificates do
- Always have at least one backup pin before certificate renewal
- Remove old pins after confirming new certificate works

### Pin Backup
- Backup pins should use a different CA or key pair
- Store backup pins in the app even if not yet deployed
- Test backup pins before emergency rotation

### Network Security Config

The app uses a custom Network Security Config that:
- Disables cleartext traffic (except for development)
- Requires certificate validation
- Supports user-installed CAs only in debug builds

## Troubleshooting

### "Certificate pinning failure" Error

The server's certificate doesn't match any configured pin.

**Solution:**
1. Check that the pin is correct
2. Verify the hostname pattern matches the URL
3. Add the new pin via Settings

### "SSLHandshakeException"

The server certificate is invalid or untrusted.

**Solution:**
1. Verify the server is using a valid certificate
2. Check system time is correct
3. Try accessing the server from a browser

### Pin Not Being Enforced

Pins are only enforced if:
1. Pinning is enabled in Settings
2. At least one pin exists for the hostname
3. The app is not in debug mode with pinning disabled

## Testing

### Test with Wrong Pin

Add a fake pin and verify the connection fails:

```kotlin
pinRepository.addPin("your-server.com", "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=")
```

Expected: Connection fails with "Certificate pinning failure"

### Test with Correct Pin

Add the correct pin and verify connection succeeds:

```kotlin
pinRepository.addPin("your-server.com", "rE2/zFR1L5z0U4Y8Jq/x8O3K8n9...")
```

Expected: Connection succeeds

## Migration from Unpinned

To migrate an existing deployment to use certificate pinning:

1. Compute the pin of your current certificate
2. Deploy app update with the pin included as default
3. Users will automatically use pinning on next update
4. Monitor for connection issues

## References

- [OkHttp CertificatePinner Documentation](https://square.github.io/okhttp/4.x/okhttp/okhttp3/-certificate-pinner/)
- [OWASP Certificate Pinning Guide](https://owasp.org/www-community/controls/Certificate_and_Public_Key_Pinning)
- [RFC 7469 - HTTP Public Key Pinning (HPKP)](https://tools.ietf.org/html/rfc7469)
