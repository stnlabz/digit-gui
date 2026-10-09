# Digit Desktop (native Windows GUI)

Digit Desktop is a small, native Win32 C client for **Digit-native, IRC-like communications**. It is **not an IRC client or IRC protocol implementation**. Digit drives the communications service; the Windows GUI is a client of the authenticated Interface API.

**Current source lineage:** the running GUI still identifies as **1.6.11**. The operator reported a Windows **BUILD GREEN** and supplied live screenshots on 2026-10-09. Interface **1.6.11** subsequently reported **46 scope-parser tests passed** and **39 historical regression suites passed**, with zero failures. Those test results do not by themselves prove a 1.6.11 module hotload.

**Principles:** Small. Deterministic. Easy to use. Clear distinction between account, project membership, SA authority and channel participation.

## Build

Run from an MSVC command-line environment with `cl.exe` available:

```cmd
git pull
build.cmd
```

The build invokes its self-test; **BUILD GREEN** is reported by the operator. Executable: `build\digit-gui.exe`. No Visual Studio solution or project file is required.

## Connection

The GUI communicates with Digit through the HTTPS Interface service on TCP port `8081`, validating the configured host and its certificate trust. Connection settings are stored in `digit.conf` next to the executable:

```text
host=digit.stn-labz.com
port=8081
```

Use the actual authorized endpoint and certificate configuration for your deployment. The GUI does not connect directly to llama.cpp or Digit Core internals.

## Current user experience

- **Organizations and projects** appear as separate, server-authorized navigation scopes (observed STN-LABZ and Team ChAoS).
- **Channels** use `#` visual prefixes, such as `#General`, `#Security` and `#Alerts`. The prefix is display-only; stored channel names, IDs and permissions do not change.
- **Conversation** in the center shows retained channel messages and submits requests to Digit from the selected authorized channel.
- **Digit — Private Chat** is a separate, authenticated one-on-one view using `POST /ask`. The current GUI view is transient: there is no private-history retrieval or retained personal memory yet. No human-to-human DMs exist. Switching away clears the private display; private responses are not appended to shared channels.
- **Users** at right currently shows an SA-authorized *restricted project-member directory projection*, **not** authenticated online/channel presence. An `[SA]` marker denotes independently verified organization SA authority, not live presence.
- **Digit [AI]** appears in the Users panel for `#General` as an agent participant. **Digit can be invoked from any authorized channel** using the existing request path. Inclusion in the list does not grant additional authority.
- **Administration** provides organization-scoped SA roster controls, project listing, scoped channel management and a Project Members window. Member details independently show restricted membership, account-active verification and SA verification.
- **Alerts** are organization-scoped operational communications, not a claim that Digit can already interpret every alert or prescribe qualified repairs.

Observed GUI behavior: `#General` greetings receive replies; several broader identity/channel questions return a generic unable-to-interpret response. The status line has sometimes remained at `Waiting for Digit...` after a reply. These limitations remain open.

## Interface endpoints in use

- `GET /health`: service health.
- `GET /channels`: channels visible through established server authorization.
- `POST /admin/channels`: scoped channel creation, subject to SA and project policy.
- `GET /channels/{id}/messages`: retained channel history.
- `POST /channels/{id}/ask`: channel-selected request to Digit.
- `POST /ask`: authenticated Digit-only private request. No private-history persistence is claimed.
- `GET /admin/dashboard`: authorized aggregate dashboard.
- `POST /admin/sa`: organization SA roster and authorized changes.
- `POST /admin/project-members`: restricted member directory with separately verified account-active and SA status.
- Scoped project and security-administration endpoints remain server-authoritative.

The GUI must not infer privileges from list labels, displayed roles or chat messages.

## Near-term roadmap

1. **Real channel presence:** authenticated channel join/leave/expiry and organization-isolated user visibility; keep human, SA and agent identities distinct.
2. **Context-aware conversation:** pass validated speaker, organization, project, channel and authorized message history to Digit's interpretation path.
3. **Intent handling:** correctly handle greetings, identity and channel questions, follow-ups, commands and unsupported tasks without invented capabilities.
4. **Grounded explanations:** use authorized evidence to interpret known errors and make bounded, tested recommendations; do not perform actions without the required permission.
5. **GUI behavior and qualification:** repair waiting/ready state and validate navigation, denial states, multi-org boundaries, history safety and regressions.

The comprehensive development roadmap is maintained in [Digit Core's roadmap](https://github.com/stnlabz/digit/blob/main/docs/ROADMAP.md). This list is **planned work**, not a claim that autonomous diagnostic or conversational capabilities are complete.

*Engineering systems worthy of trust when trust matters most.*
