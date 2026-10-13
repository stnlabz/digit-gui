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
