# Custom realms (Battle.net gateways)

d2bsng can add an extra Battle.net server ("gateway") for the client to connect
to, from the command line, and lets scripts enumerate the realms the client can
reach. This documents how they are stored, injected into the game, and
enumerated.

## What the game calls a "realm"

Two distinct concepts:

- **Gateway** - a Battle.net Chat Server (BNCS). The game reads its gateway list
  from the Windows registry and shows it in the login server selector. This is
  the list users edit with gateway-editor tools, and the one d2bsng injects into
  (in memory only - it never writes the registry; see below).
- **Realm** - an MCP game server behind a gateway, announced by BNCS after login
  (`MAINMENU_QueryAndBuildRealmList`). It is not client-configurable.

"Adding a realm to connect to" in practice means adding a gateway, so that is
what the framework does. The user-facing names (`-realm`, `getRealms()`) say
"realm" because that is the botting-community term for it.

## Storage

`config::RealmRegistry` (in `core`) is the store of the `-realm` additions
(`{name, host}`). It is seeded once at backend init and is read-only thereafter -
realms are added on the command line, not from scripts.

## Command line

```
-realm name:host             # repeatable; --realm is also accepted
```

The client always dials a server on the fixed BNCS port 6112 (as real
Battle.net does), so `-realm` takes only a host, no port. Parsed in
`LaunchOptions` into raw specs; `hooks::realms::Init()` feeds each to
`RealmRegistry::AddSpec` before the client reads its server list.

## In-memory injection

The 1.14d client reads its gateway list (in `BNGatewayAccess::Load`, RVA
0x51871d) from:

```
HKCU\Software\Battle.net\Configuration
  value "Diablo II Battle.net gateways"   (REG_MULTI_SZ; "Override Battle.net
                                            gateways" wins if present)
```

The multistring is: `version`, `selected-index`, then a `host, GMT-zone,
display-name` triple per gateway, e.g.

```
"1009", "04", "uswest.battle.net","8","U.S. West", ... , "127.0.0.1","0","Localhost"
```

The version must be >= 1000 or the client discards the list and rebuilds the
defaults from the MPQ `DATA\GLOBAL\gateways.txt`.

The registry value is **shared by every game instance**, so d2bsng does not write
it. Instead `backends/lod114d/hooks/Realms.cpp` Detours the two Storm
registry helpers the client uses (resolved via `imports::storm`), filtered to the
gateway value names:

- **`SSTR_RegistryReadValueEx`** (RVA 0x14DE0) - reads the real blob via the
  trampoline, splices the registry realms in by display name (overwriting an
  existing gateway's host or appending a new one), bumps a too-low version, and
  hands the merged blob back. The client parses defaults + custom in memory; the
  on-disk value is untouched. If the value is absent, the read passes through so
  the client seeds its defaults (which the next read then injects into).
- **`RegStoringKeysConfiguration`** (RVA 0x15000) - strips the registry realms
  from any gateway-list write the client makes (e.g. recording a gateway
  selection), so nothing of ours is ever persisted to the shared value.

The result: each instance sees its own realms; the registry only ever holds
D2's own gateways. The detours are installed from `HookManager` - only when at
least one `-realm` was given (a no-op otherwise, like the SOCKS5 hook without
`-proxy`) - before the client's first gateway read (user-triggered, on "Connect
to Battle.net").

Realms passed via `-realm` are present from the first gateway read (realms are
fixed at launch, so there is no mid-session refresh concern).

## Enumeration

`game::GetRealms()` (declared in the contract `game/GameHelpers.h`, implemented
in `hooks/Realms.cpp`) returns the full realm list as `{name, host}` records,
backing the `getRealms()` JS global.

It reads the client's **live, parsed gateway list** from the `BNGatewayAccess`
singleton's blob (`imports::bnclient::gBNGatewayAccess`, `+0x10`) rather than the
registry, so it reflects exactly what the client has loaded (including the
in-memory injection). Because that buffer is (re)allocated by the client's
`Load` / `SaveAndUnload` (which run on the Battle.net connect-worker thread), the
read is marshalled onto the game thread via `GameThread::Execute`, where the menu
reads it - this narrows but does not fully close a torn-read window during a live
(re)load. When the client has not loaded its gateways yet (blob pointer still
null), it falls back to reading the registry directly. The `-realm` additions are
then overlaid by name, so they appear even if the injection hook did not run.

## Port

D2's gateway host field carries no port; the client always dials BNCS on 6112
(`GetDnsResults`, RVA 0x51bee0, only runs `inet_addr` / `gethostbyname` on the
host). `-realm` therefore takes only a host - a port cannot be honoured without
detouring `connect`, and private servers run BNCS on 6112 like real Battle.net.

## JS API

A single global function (`api/globals/MenuFunctions.cpp`) exposes the
enumeration; realms are added on the command line (`-realm`), not from scripts:

```js
getRealms()
// -> [ { name: "U.S. West", host: "uswest.battle.net" },
//      ...,
//      { name: "MyServer",  host: "1.2.3.4" } ]
```
