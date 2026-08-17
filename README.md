# English–Vietnamese Input Indicator

`nkarl.en-vi-indicator` is an Omarchy Quattro bar widget for an Fcitx 5 setup
that switches between US English and Vietnamese Lotus:

- `keyboard-us` displays `EN`.
- `lotus` displays `VI`.
- An unavailable or unexpected state displays `??`.

The plugin intentionally recognizes only these two input-method IDs.

## Prerequisites

### Vietnamese input

Install the Fcitx framework and application integration packages from the
Arch repositories:

```sh
sudo pacman -S fcitx5 fcitx5-gtk fcitx5-qt
```

Install Lotus from the AUR:

```sh
yay -S fcitx5-lotus-bin
```

`fcitx5-configtool` is optional but useful for adding and configuring input
methods graphically:

```sh
sudo pacman -S fcitx5-configtool
```

The expected Fcitx profile contains both methods in the same group, with IDs
matching the widget's mapping:

```ini
[Groups/0]
Default Layout=us
DefaultIM=lotus

[Groups/0/Items/0]
Name=keyboard-us

[Groups/0/Items/1]
Name=lotus
```

Fcitx must also be running in the graphical session. This machine starts it
from the Hyprland environment configuration with:

```ini
env = XMODIFIERS,@im=fcitx
exec-one = fcitx5 -d --replace
```

Confirm the active method before troubleshooting the widget:

```sh
fcitx5-remote -n
```

The output should be either `keyboard-us` or `lotus`.

### Widget runtime and build dependencies

The checked-in `fcitx-state-monitor` binary uses `libsystemd`. On Arch,
`systemd-libs` supplies both the runtime library and the `sd-bus.h` header.
Rebuilding also requires a compiler and `pkg-config`, provided through the
`base-devel` toolchain.

```sh
sudo pacman -S systemd-libs base-devel
```

## Architecture

The manifest declares both `bar-widget` and `service`. Quattro creates one bar
widget per monitor but only one service for the shell session:

```text
Fcitx Controller1.CurrentInputMethod()
                  ↓
      fcitx-state-monitor (one process)
                  ↓ change-only stdout
          Service.qml (one instance)
                  ↓ shared inputMethod property
        BarWidget.qml (one per monitor)
```

`Service.qml` owns the monitor process and stores its latest output.
`BarWidget.qml` obtains the shared service through:

```qml
bar.shell.serviceFor("nkarl.en-vi-indicator")
```

Adding monitors therefore creates only additional QML views. It does not add
monitor processes or D-Bus clients.

### Why the native monitor exists

Fcitx 5.1.21 exposes the authoritative state through this D-Bus method:

```text
Service:   org.fcitx.Fcitx5
Path:      /controller
Interface: org.fcitx.Fcitx.Controller1
Method:    CurrentInputMethod
```

The interface does not provide a corresponding
`CurrentInputMethodChanged` signal, and the installed Quickshell build does
not expose a general-purpose QML D-Bus client. Pure QML therefore cannot query
this method directly or subscribe to a native state-change signal.

`fcitx-state-monitor` is a small C program using `sd-bus`. It remains alive,
checks `CurrentInputMethod` every 750 milliseconds, sleeps on the bus between
checks, and writes to stdout only when the value changes. This avoids creating
a new `fcitx5-remote` process on every check.

If Fcitx is temporarily unavailable, the helper emits nothing and preserves
the last valid widget state. Later checks resume normally when Fcitx returns.

## Resource use

The singleton helper measured as follows on the development machine:

```text
RSS:             about 3.0 MB
Private memory:  about 184 KB
PSS:             about 290 KB
CPU while idle:  0.0% at the sampled resolution
```

RSS includes shared `libsystemd`, `libc`, and loader pages, so it overstates
the incremental cost. The private-memory and PSS figures are more useful. The
singleton service keeps this cost constant as monitors are added.

## Decision record

### Original QML polling

The first implementation ran `fcitx5-remote -n` every 250 milliseconds. It
returned the correct input-method ID but created up to four processes per
second for every monitor. Guarding overlapping calls and using a longer
interval would reduce the load, but would retain unnecessary process churn.

### System-tray icon inference

An alternative watched `SystemTray.items` and treated an icon name containing
`lotus` as Vietnamese. That would reuse Quickshell's StatusNotifier D-Bus
connection and require no helper.

It was rejected because a tray icon is presentation state rather than the
authoritative Fcitx state. It also requires the tray item to be registered and
its icon names to remain stable. During investigation, the live
StatusNotifier watcher had zero registered items, so this implementation would
have remained unavailable.

### Persistent helper per monitor

The first persistent helper used Qt Core and Qt D-Bus. It removed repeated
process launches but consumed about 18 MB RSS per instance, and
`BarWidget.qml` started one instance for every monitor.

Rewriting it with `sd-bus` reduced each instance to about 3 MB RSS. Moving
ownership into Quattro's singleton plugin service then reduced the process
count to one per shell session, independent of monitor count.

## Files

- `manifest.json` declares the combined service and bar widget.
- `Service.qml` owns the singleton helper and parses its output.
- `BarWidget.qml` maps shared Fcitx state to `EN`, `VI`, or `??`.
- `fcitx-state-monitor.c` contains the helper source.
- `fcitx-state-monitor` is the compiled runtime helper.

## Build and reload

From this plugin directory, rebuild the helper with:

```sh
cc -std=c17 -O2 -Wall -Wextra -pedantic -fPIE -pie \
  fcitx-state-monitor.c -o fcitx-state-monitor \
  $(pkg-config --cflags --libs libsystemd)
```

Validate the manifest, rescan plugins, and restart the shell after changing
the manifest or helper:

```sh
omarchy plugin validate nkarl.en-vi-indicator
omarchy-shell shell rescanPlugins
omarchy restart shell
```

## Troubleshooting and earlier failures

The widget initially disappeared after `BarWidget.qml` lost its `QtQuick`,
`Quickshell.Io`, and `qs.Ui` imports. Quickshell reported:

```text
StdioCollector is not a type
```

Restoring the imports fixed the load failure. Process handling later moved to
`Service.qml`, so `BarWidget.qml` no longer needs `Quickshell.Io`.

Another failure came from a visible backup directory named
`nkarl.en-vi-indicator.bak`. It contained an older manifest with the same
plugin ID. Quattro scanned it after the active directory and replaced the
current manifest in its registry. The backup is now named
`.nkarl.en-vi-indicator.bak`; Quattro ignores hidden plugin directories.

Do not keep visible backup directories under
`~/.config/omarchy/plugins/` if they contain a valid manifest with the same ID
as an active plugin.

Useful diagnostics:

```sh
# The registry should report both "bar-widget" and "service".
omarchy plugin list --json | jq '.[] | select(.id == "nkarl.en-vi-indicator")'

# Exactly one monitor should exist regardless of display count.
pgrep -fc '^/home/kyle/.config/omarchy/plugins/nkarl.en-vi-indicator/fcitx-state-monitor$'

# Check the authoritative current input method.
fcitx5-remote -n

# Inspect recent plugin errors.
journalctl --user --since '10 minutes ago' --no-pager | \
  rg 'nkarl.en-vi-indicator|Plugin widget.*failed|service plugin'
```
