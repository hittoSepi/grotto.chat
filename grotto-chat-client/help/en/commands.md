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

## Voice

`/call <name>`             call a user  
`/accept`                  answer an incoming call  
`/hangup`                  end call  
`/voice`                   toggle voice channel on/off  
`/mute`                    toggle microphone mute  
`/deafen`                  mute all incoming audio  
`/vmode`                   toggle voice mode (`PTT` / `VOX`)  

## Security

`/trust <name>`            mark a user's key as trusted  

## Tools

`/search <query>`          search message history  
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

---

Tip: Commands autocomplete with the `Tab` key. Type `/` and press Tab.
In the `F3` files panel, use `r` to refresh and `o` to open downloads.
