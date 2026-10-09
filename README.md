# Digit GUI 1.5.4

Version 1.5.4 targets Digit Interface 1.5.4. GUI integration and runtime qualification are ongoing.

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

Connection settings are read from `digit.conf` beside `digit-gui.exe`:

```text
host=127.0.0.1
port=8081
```

The GUI does not connect directly to llama.cpp or Digit Core internals.

## Interface API

Digit GUI uses:

- `GET /health` for connection state.
- `GET /channels` for persistent discussion channels.
- `POST /channels` to create a channel.
- `GET /channels/{id}/messages` for channel history.
- `POST /channels/{id}/ask` for channel-scoped conversation with Digit.
- `GET /alerts?unacknowledged=1` for active operator alerts.
- `POST /alerts/{id}/acknowledge` to acknowledge an alert without deleting its historical record.

## GUI

The main window contains three operational areas:

- **Channels** on the left. Selecting a channel loads its retained discussion history. Type a channel name into the input field and use **New Channel** to create another room.
- **Conversation** in the center. Questions are sent to the currently selected channel and both sides of the conversation are retained by Digit.
- **Operator Alerts** on the right. Unacknowledged Core/module alerts remain visible until the operator explicitly acknowledges them.

The status line reports the active channel and current unacknowledged alert count.

Digit GUI remains a thin client: channel state, conversation persistence, alerts, and acknowledgment state are owned by Digit Core and exposed through the Interface module.
