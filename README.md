# English–Vietnamese Input Indicator

`nkarl.en-vi-indicator` is an Omarchy Quattro bar widget that shows the current
Fcitx 5 input method:

- `keyboard-us` → `EN`
- `lotus` → `VI`
- unavailable or unexpected state → `??`

The widget is deliberately limited to these two input methods because that is
the configured Fcitx Lotus setup on this machine.

## Current architecture

The plugin declares both `bar-widget` and `service` kinds. Quattro creates a
`BarWidget.qml` instance for each monitor, but creates `Service.qml` only once
for the shell session.

```text
Fcitx Controller1.CurrentInputMethod()
                  ↓
      fcitx-state-monitor (one process)
                  ↓ stdout, only when state changes
          Service.qml (one instance)
                  ↓ shared inputMethod property
        BarWidget.qml (one per monitor)
```

`Service.qml` owns the native monitor process and publishes its latest output
as `inputMethod`. Every bar instance obtains that same service through
`bar.shell.serviceFor("nkarl.en-vi-indicator")`. Adding monitors therefore adds
only small QML views; it does not add monitor processes or D-Bus clients.

## Why a native monitor remains necessary

Fcitx 5.1.21 exposes the authoritative current input method through:

```text
Service:   org.fcitx.Fcitx5
Path:      /controller
Interface: org.fcitx.Fcitx.Controller1
Method:    CurrentInputMethod
```

That interface does not expose a corresponding
`CurrentInputMethodChanged` signal. The installed Quickshell build also does
not provide a general-purpose QML D-Bus client. Consequently, pure QML cannot
both query this method directly and receive a native change notification.

`fcitx-state-monitor` is a small C program using `sd-bus`. It stays alive,
queries the D-Bus method every 750 milliseconds, sleeps on the bus between
queries, and writes to stdout only when the value changes. This still checks
state periodically, but it avoids repeatedly creating `fcitx5-remote`
processes.

The helper automatically tolerates Fcitx being temporarily unavailable. A
failed call produces no output, preserving the last valid state; later calls
resume normally when Fcitx returns.

## Approaches considered

### Launching `fcitx5-remote` from a QML timer

The original widget ran:

```text
fcitx5-remote -n
```

every 250 milliseconds. This returned the correct input-method ID, but created
up to four processes per second for every bar instance. On a two-monitor setup,
Quattro instantiated two widgets and doubled that work. Guarding overlapping
queries and using a longer interval would improve it, but repeated process
creation was unnecessary.

### Reading the Fcitx system-tray icon

Another implementation watched `SystemTray.items` and inferred Vietnamese
when the icon name contained `lotus`. This would have reused Quickshell's
StatusNotifier D-Bus connection and required no helper.

It was rejected because it observes presentation state rather than Fcitx's
authoritative input-method state. It also depends on the Fcitx tray item being
registered and on its icon naming convention. During investigation, the live
StatusNotifier watcher reported zero registered items, so that implementation
would have remained at its unavailable state.

### A Qt D-Bus helper inside every widget

The first persistent-helper prototype used Qt Core and Qt D-Bus. It removed
process-launch churn, but each Quattro bar instance started its own helper. On
the tested system, each process reported about 18 MB RSS.

The helper was rewritten against `sd-bus`, reducing each process to about 3 MB
RSS. More importantly, moving ownership from `BarWidget.qml` to the Quattro
plugin service reduced the process count from one per monitor to one per shell
session.

## Resource measurements

For the current `sd-bus` helper, the measured single-process footprint was:

```text
RSS:             about 3.0 MB
Private memory:  about 184 KB
PSS:             about 290 KB
CPU while idle:  0.0% at the sampled resolution
```

RSS includes shared `libsystemd`, `libc`, and dynamic-loader pages and therefore
overstates the incremental memory cost. Private memory and PSS better describe
the actual additional system cost. Because the helper is now owned by the
singleton service, this cost does not grow with the monitor count.

## Files

- `manifest.json` declares the combined service and bar-widget plugin.
- `Service.qml` owns the single helper and parses change-only stdout.
- `BarWidget.qml` maps the shared Fcitx state to `EN`, `VI`, or `??`.
- `fcitx-state-monitor.c` is the monitor source.
- `fcitx-state-monitor` is the compiled helper used at runtime.

## Building the helper

The build requires a C compiler, `pkg-config`, and the systemd development
headers already present on Omarchy:

```sh
cc -std=c17 -O2 -Wall -Wextra -pedantic -fPIE -pie \
  fcitx-state-monitor.c -o fcitx-state-monitor \
  $(pkg-config --cflags --libs libsystemd)
```

After rebuilding or changing the manifest, rescan the plugins and restart the
shell if hot reload retains an older component:

```sh
omarchy-shell shell rescanPlugins
omarchy restart shell
```

Validate the plugin with:

```sh
omarchy plugin validate nkarl.en-vi-indicator
```

## Failure history and troubleshooting notes

The widget initially disappeared because `BarWidget.qml` had lost its
`QtQuick`, `Quickshell.Io`, and `qs.Ui` imports. Quickshell logged:

```text
StdioCollector is not a type
```

Restoring the imports fixed that load failure. Later revisions removed
`Quickshell.Io` from `BarWidget.qml` because process handling moved into
`Service.qml`.

A second issue came from a sibling backup directory named
`nkarl.en-vi-indicator.bak`. It contained an older manifest with the same
plugin ID. Quattro scans every non-hidden top-level plugin directory, and the
backup was read after the active directory, silently replacing the new
manifest in the registry. The backup was preserved as
`.nkarl.en-vi-indicator.bak`; leading-dot directories are ignored by the local
plugin scanner.

Avoid keeping visible backup directories under
`~/.config/omarchy/plugins/` when they contain a valid `manifest.json` with the
same ID as an active plugin.

Useful checks:

```sh
# Confirm the registry sees both plugin kinds.
omarchy plugin list --json | jq '.[] | select(.id == "nkarl.en-vi-indicator")'

# Confirm exactly one monitor exists, regardless of display count.
pgrep -fc '^/home/kyle/.config/omarchy/plugins/nkarl.en-vi-indicator/fcitx-state-monitor$'

# Read the authoritative current state directly.
fcitx5-remote -n

# Inspect recent load errors.
journalctl --user --since '10 minutes ago' --no-pager | \
  rg 'nkarl.en-vi-indicator|Plugin widget.*failed|service plugin'
```
