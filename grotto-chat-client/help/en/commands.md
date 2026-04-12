# Commands

## Basics

`/msg <name> <text>`       send a private message (alias: `/w`)  
`/me <action>`             send an action message (* Rausku drinks coffee)  
`/nick <new_name>`         change nickname  

## Channels

`/join <#channel>`         join a channel (name starts with #, alias: `/j`)  
`/part [#channel]`         leave current or specified channel (alias: `/p`)  
`/names`                   list online users (alias: `/ns`)  
`/whois <name>`            show user info  
`/away [reason]`           set yourself away  
`/afk [reason]`            alias for `/away`  
`/back`                    return to online status  
`/dnd [reason]`            set yourself to do not disturb  

## Voice

`/call <name>`             call a user  
`/accept`                  answer an incoming call  
`/hangup`                  end call  
`/voice`                   toggle voice channel on/off  
`/voicetest`               toggle a local microphone loopback self-test  
`/mute`                    toggle microphone mute  
`/deafen`                  mute all incoming audio  
`/vmode`                   cycle voice mode (`Toggle to Talk` / `Push to Talk` / `VOX`)  

## Security

`/trust <name>`            mark a user's key as trusted  

## Tools

`/search <query>`          search message history (supports FTS5 syntax)  
  Examples:  
    /search hello            search for "hello" in current channel  
    /search hello world      search for messages containing both words  
    /search "hello world"    search for exact phrase  
    /search user:Alice       search messages from user Alice  
    /search day:2026-04-09   search messages from specific date  
    /search hello user:Bob   combined search (word + user filter)  
`/clear`                   clear message view  
`/settings`                open settings screen  
`/version`                 show client and server version (alias: `/ver`)  
`/status`                  show connection status (alias: `/st`)  
`/diag ui`                 show UI/clipboard/graphics diagnostics  
`/help [topic]`            show this help or a specific topic (alias: `/h`)  
`/reload_help`             reload help files from disk (alias: `/rh`)  

## Connection

`/disconnect`              disconnect from server  
`/quit`                    close Grotto for good (alias: `/q`)  

## File Transfer

`/upload <path>`           upload a file (alias: `/up`)  
`/download <id> [path]`    download a file (alias: `/dl`)  
`/transfers [limit]`       show recent file transfer state (alias: `/xfers`)  
`/files`                   refresh files for the active channel/DM and open the files panel (alias: `/ls`)  
`/downloads`               open the local downloads folder (alias: `/dir`)  
`/quota`                   show your current file storage usage, limits, and remaining space
`/rmfile <id>`             delete one of your uploaded files (or a channel file if you are an operator)  

---

Tip: Commands autocomplete with the `Tab` key. Type `/` and press Tab.
*In the `F3` files panel, use `Ctrl+F` to filter, `s` to change sorting, `r` to refresh, `Del` to remove, and `o` to open downloads.*
