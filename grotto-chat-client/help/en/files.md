# Files

## Sending

`/upload <path>` sends a file to the active channel or user.
`/transfers [limit]` shows current and recent transfer state.
`/files` refreshes the file list for the active channel or DM and opens the files panel.
`/downloads` opens the local downloads folder.
`/quota` shows your current storage usage, limits, and remaining space.
`/rmfile <id>` deletes a file you uploaded.

Examples:
```
/upload C:\pictures\meme.png
/upload ~/documents\secrets.txt
```

## Receiving

When someone sends a file, Grotto shows a notification. Use `/download` to save the file.

Press `F3` to open the files panel for the current channel or DM. You can:
- browse the available files
- move selection with arrow keys when the input is empty
- press `Enter` to download the selected file
- press `Del` to remove the selected file if you are allowed to delete it
- press `r` to refresh the list manually
- press `o` to open the local downloads folder
- double-click a file with the mouse to download it directly

## Security

Files are encrypted the same way as messages. The server only forwards chunks without seeing the content.

If the server advertises file policy, the client blocks uploads that are obviously too large or use disallowed MIME types before sending them.
Live quota enforcement still happens on the server, so an upload can still fail later if current reserved usage has changed.
If the server advertises storage quotas, `/quota` and the files panel show usage, limits, and remaining space.
