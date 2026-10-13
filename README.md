# Digit GUI — Single Chat Window

Native Windows Win32 C client. The launched application now presents **one conversation view** and a message composer: no channel list, projects, users sidebar, alerts, administration, login, passwords, or bearer tokens.

Build from an MSVC Developer Command Prompt:

```cmd
git pull
build.cmd
build\digit-gui.exe
```

The new UI is implemented in `src/chat.c`; the previous multi-channel implementation remains in `src/main.c` as non-built historical source. The client currently targets `digit.stn-labz.com:8081` over validated WinHTTP TLS and submits text to `POST /ask` without credentials. This **is not yet a functioning Digit conversation integration**: the current independently deployed Digit Interface 0.1.2 is TLS transport only, accepts a handshake, and closes it without HTTP handling. A separately implemented and Router-connected application protocol is needed before a response can be received. The GUI reports errors rather than fabricating replies.

The GUI build self-test verifies JSON answer extraction. Windows compilation and user end-to-end testing are still outstanding. No server, Core, Router, or Interface change is included in this GUI-only revision.

## Scoped diagnostics (awaiting operator verification)

This GUI retains the Interface-issued `X-Digit-Conversation` value in process memory and echoes it for subsequent `/ask` requests. No login, credential storage, or administrative authorization is added. The token expires after 30 minutes idle; restarting the application starts a new conversation. Ask 0.1.3 delegates `diagnose my last request` to DSD 0.4.3 with only that conversation's previous dispatch outcome. The DSD result is a bounded observation, not a system-wide log search or a guaranteed root-cause diagnosis. Server requires Interface 0.1.4. Windows build and end-to-end acceptance remain outstanding.
