# Files

## Sending

`/upload <path>` sends a file to the active channel or user.
`/transfers [limit]` shows current and recent transfer state.

Examples:
```
/upload C:\pictures\meme.png
/upload ~/documents\secrets.txt
```

## Receiving

When someone sends a file, Grotto shows a notification. Use `/download` to save the file.

## Security

Files are encrypted the same way as messages. The server only forwards chunks without seeing the content.

If the server advertises file policy, the client blocks uploads that are too large or use disallowed MIME types before sending them.
