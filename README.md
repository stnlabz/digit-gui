# Digit GUI

Small native Windows client for interacting with Digit.

## Build

Use an MSVC command-line environment with `cl.exe` available:

```cmd
build.cmd
```

Every build runs the required self-test. A failed self-test fails the build.

Output:

```text
build\digit-gui.exe
```

No Visual Studio solution or project files are required.

## Digit connection

Digit GUI talks only to the Digit Interface API on TCP port `8081`.

Set the server address before launching:

```cmd
set DIGIT_HOST=192.0.2.10
build\digit-gui.exe
```

If `DIGIT_HOST` is not set, the client defaults to `127.0.0.1`.

The initial client uses:

- `GET /health` for connection state.
- `POST /ask` with `text/plain` for questions.

The GUI does not connect directly to llama.cpp.

## Initial scope

The first milestone intentionally contains only what is needed for daily development interaction:

- conversation display;
- question input;
- Send button;
- Digit answer display;
- Connected / Offline status;
- mandatory build-time self-test.
