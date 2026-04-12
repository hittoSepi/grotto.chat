# Voice

## Calling

`/call <name>`       start a private call with a user  
`/accept`            answer when someone calls you  
`/voicetest`         toggle a local microphone loopback test  

## During a Call

`/mute`              toggle your microphone  
`/deafen`            mute all incoming audio  
`/vmode`             cycle between toggle-to-talk, push-to-talk, and VOX  
`/hangup`            end the call  

In `Toggle to Talk` mode, press your configured talk key to start or stop transmitting. In `Push to Talk` mode, hold the talk key while you speak. In `VOX` mode, audio goes out automatically when you speak.

## Settings

Open `/settings` and select the correct audio devices. If you can't hear anything, check that the right output device is selected.

`/voicetest` opens the same local capture/playback path used by calls, but keeps everything on your machine. In `Toggle to Talk`, press your talk key once to start or stop monitoring. In `Push to Talk`, hold the talk key while listening. In `VOX`, speak normally and the client will loop your microphone back to the selected output device.

---

Note: Audio goes directly peer-to-peer via WebRTC. The server doesn't hear your calls.
