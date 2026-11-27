# NetBoX

Redes de Computadores 2026/2027 (LEIC-A) - Group 52

## Group members

| Name | IST number |
| ------ | ------------ |
| `Diogo Barata` | `95563` |
| `Gustavo` | `` |
| `Taha` | `` |

## Build

```bash
make          # builds the "user" executable in the current directory
make clean    # removes the executable
make debug    # builds with -g and sanitizers (development only)
```

Written in C (C11). Tested in the LT5 environment (gcc).

## Run

```bash
./user -m peerport [-n DSIP] [-p DSport]
```

| Flag | Required | Description |
| ------ | ---------- | ------------- |
| `-m peerport` | yes | TCP port where the application accepts requests from other users |
| `-n DSIP` | no | Directory Server IP. Default: `193.136.138.142` (outside LT5). Inside LT5 use `192.168.1.1` |
| `-p DSport` | no | DS port. Default: `59000` |

Example:

```bash
./user -m 58052 -n 193.136.138.142
```

## Commands (Phase II)

| Command | Description |
| --------- | ------------- |
| `login UID password` | Logs in; registers the user if new |
| `logout` | Ends the session |
| `unregister` | Deletes the user (also logs out) |
| `publish filename label` | Publishes a file from the current directory |
| `remove filename` | Removes one of the user's publications |
| `list` | Lists the known resources (max. 50) |
| `versions filename` | Lists who publishes the file (over TCP) |
| `exit` | Quits (requires a prior logout) |

Notes:

- `list` and `versions` do not require login.
- Ctrl+C and Ctrl+D with an active session log out automatically.
- The `where` and `get` commands are left for later phases.

## Code structure

```text
Makefile
README.md
src/
├── client/
│   ├── main.c             arguments, session, command loop
│   ├── commands.c/.h      one function per command (protocol + messages)
│   ├── display.c/.h       output formatting for list and versions
│   └── network/
│       ├── udp_client.c/.h   UDP communication with the DS
│       └── tcp_client.c/.h   TCP communication with the DS
└── common/
    ├── config.h           constants (sizes, limits, timeouts)
    ├── validation.c/.h    UID, password, filename and label validation
    └── io.c/.h            complete read/write helpers on descriptors
```

The `server/` folder of the suggested structure will be used in later phases (TCP peer server). C has no exceptions, so there is no exceptions folder: errors are handled with return values and `errno`.

## Implementation decisions

- **UDP:** socket created at startup and reused; 5 s timeout and up to 3 attempts per request (retransmission).
- **TCP:** one connection per request, with 5 s timeouts; reading until `\n` and writing are done in loops (partial read/write).
- `EINTR` is handled in blocking read and write calls.
- Fields are validated locally before sending; `publish` checks that the file exists.
- The session state (UID/password) is only stored or cleared when the DS confirms the operation.
- `SIGPIPE` is ignored; the application does not terminate on a network failure or unexpected reply, it informs the user.

## Known limitations

- The UDP reply sender address is not checked against the DS.
- Retransmitting non-idempotent requests (`LOU`, `REM`, `UNR`) may return `NOK`/`NLG` if only the reply was lost.
- The TCP `connect` relies on `SO_SNDTIMEO` being honored.
