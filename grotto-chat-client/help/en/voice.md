# Voice

## Calling

`/call <name>`       start a private call with a user  
`/accept`            answer when someone calls you  

## During a Call

`/mute`              toggle your microphone  
`/deafen`            mute all incoming audio  
`/vmode`             switch between push-to-talk and VOX  
`/hangup`            end the call  

In PTT mode, press your configured PTT key (`F1` by default) to start or stop transmitting. In VOX mode, audio goes out automatically when you speak.

## Settings

Open `/settings` and select the correct audio devices. If you can't hear anything, check that the right output device is selected.

---

Note: Audio goes directly peer-to-peer via WebRTC. The server doesn't hear your calls.
