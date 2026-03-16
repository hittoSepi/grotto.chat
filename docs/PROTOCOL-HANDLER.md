# Grotto Protocol Handler (ircord://)

This document explains how the `ircord://` protocol works for connecting to Grotto servers directly from web browsers.

## Protocol Format

```
ircord://<host>:<port>
```

Examples:
- `ircord://chat.rausku.com:6697`
- `ircord://localhost:6667`
- `ircord://grotto.example.com`

If port is omitted, the default port `6697` is used.

## How It Works

### Web Browser Flow

1. User clicks an `ircord://` link on a webpage
2. Browser attempts to launch the registered protocol handler
3. If the Grotto client is installed, it opens and connects to the server
4. If not installed, the browser shows an error or the landing page shows help

### Landing Page Integration

The landing page includes:
- JavaScript to detect if the protocol handler is installed
- Fallback UI with manual connection instructions
- Copy-to-clipboard for server addresses

```javascript
// Try to open Grotto client
window.location.href = 'ircord://chat.example.com:6697';

// Check if it worked (page visibility)
setTimeout(() => {
  if (!document.hidden) {
    // Protocol not registered, show help
    showProtocolHelp();
  }
}, 500);
```

## Platform-Specific Registration

### Windows

The Grotto installer creates registry entries:

```reg
Windows Registry Editor Version 5.00

[HKEY_CLASSES_ROOT\grotto]
@="URL:Grotto Protocol"
"URL Protocol"=""

[HKEY_CLASSES_ROOT\grotto\shell]

[HKEY_CLASSES_ROOT\grotto\shell\open]

[HKEY_CLASSES_ROOT\grotto\shell\open\command]
@="\"C:\\Program Files\\Grotto\\grotto-client.exe\" \"%1\""
```

### macOS

Create an `Info.plist` entry in the app bundle:

```xml
<key>CFBundleURLTypes</key>
<array>
  <dict>
    <key>CFBundleURLName</key>
    <string>com.grotto.client</string>
    <key>CFBundleURLSchemes</key>
    <array>
      <string>grotto</string>
    </array>
  </dict>
</array>
```

### Linux

Create a `.desktop` file:

```ini
[Desktop Entry]
Name=Grotto
Exec=/usr/bin/grotto-client %u
Type=Application
MimeType=x-scheme-handler/grotto;
```

Register with:
```bash
xdg-mime default grotto.desktop x-scheme-handler/grotto
```

## Client Implementation

The Grotto client should:

1. Parse the URL argument when launched
2. Extract host and port
3. Connect to the server immediately
4. Skip the manual server entry step

Example CLI usage:
```bash
grotto-client ircord://chat.example.com:6697
# Equivalent to:
grotto-client --host chat.example.com --port 6697
```

## Limitations

### Web Browser Limitations

1. **No Detection**: Browsers cannot reliably detect if a protocol handler is installed
2. **No Fallback**: If the handler is missing, users see an error page
3. **Security**: Some browsers block protocol handlers from iframes or require user interaction

### Workarounds

1. **Detection Attempt**: Use `document.hidden` check after a short delay
2. **Fallback UI**: Always provide manual connection instructions
3. **Web Client**: Consider a web-based Grotto client as ultimate fallback

## Future Enhancements

### Web-Based Client

A web client could be hosted on `web.grotto.dev`:

```
https://web.grotto.dev/connect?server=chat.example.com&port=6697
```

This would:
- Work without installing anything
- Use WebSocket connection to Grotto servers
- Provide full client functionality in browser

### Progressive Web App (PWA)

A PWA could:
- Register as a protocol handler using `navigator.registerProtocolHandler()`
- Work offline
- Provide native-like experience

## Security Considerations

1. **Validate URLs**: Always validate host and port before connecting
2. **Prevent SSRF**: Don't allow internal IP addresses or localhost without explicit user confirmation
3. **HTTPS**: Landing page should use HTTPS to prevent MITM attacks
4. **CORS**: Directory service must have proper CORS headers

## Testing

Test the protocol handler:

```html
<!-- Test page -->
<a href="ircord://localhost:6697">Connect to localhost</a>
<button onclick="testProtocol()">Test Protocol</button>

<script>
function testProtocol() {
  window.location.href = 'ircord://test.example.com:6697';
}
</script>
```

## References

- [MDN: Web-based protocol handlers](https://developer.mozilla.org/en-US/docs/Web/API/Navigator/registerProtocolHandler)
- [Microsoft: Registering an Application to a URI Scheme](https://docs.microsoft.com/en-us/windows/desktop/shell/app-registration)
- [Apple: Registering Custom URL Schemes](https://developer.apple.com/documentation/uikit/inter-process_communication/allowing_apps_and_websites_to_link_to_your_content)
