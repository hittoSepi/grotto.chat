# Transfers

`/transfers [limit]` shows recent file uploads and downloads.

Examples:
```
/transfers
/transfers 5
```

Each line shows:
- direction (`upload` or `download`)
- local transfer id
- state (`queued`, `uploading`, `downloading`, `completed`, `failed`, `cancelled`)
- progress in percent and bytes
- target channel / DM or local download path

Active transfers also appear in the status bar.
