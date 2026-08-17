# Fcitx Input Method Indicator

`nkarl.en-vi-indicator` is an Omarchy Quattro bar widget for displaying,
prioritizing, and switching among the input methods in the active Fcitx 5
group. The plugin ID is retained from its original English–Vietnamese-only
version, but the current implementation supports any configured Fcitx input
method.

## Prerequisites

### Fcitx and Vietnamese Lotus

Install Fcitx and its GTK/Qt application integrations from the Arch
repositories:

```sh
sudo pacman -S fcitx5 fcitx5-gtk fcitx5-qt
```

For Vietnamese Lotus, install the AUR package:

```sh
yay -S fcitx5-lotus-bin
```

`fcitx5-configtool` is optional but useful for managing Fcitx groups and input
methods graphically:

```sh
sudo pacman -S fcitx5-configtool
```

The plugin selects from methods already present in the active Fcitx group. It
does not install input engines or edit the Fcitx profile. To make another
method appear in the add picker, first add it to the active group with
`fcitx5-configtool`, then reopen the picker. A typical English–Vietnamese
profile contains:

```ini
[Groups/0]
Default Layout=us
DefaultIM=lotus

[Groups/0/Items/0]
Name=keyboard-us

[Groups/0/Items/1]
Name=lotus
```

Fcitx must be running in the graphical session. This machine starts it from
the Hyprland environment configuration with:

```ini
env = XMODIFIERS,@im=fcitx
exec-one = fcitx5 -d --replace
```

Confirm the current input method with:

```sh
fcitx5-remote -n
```

### Helper dependencies

The checked-in `fcitx-state-monitor` binary uses `libsystemd`. On Arch,
`systemd-libs` supplies both the runtime library and `sd-bus.h`. Rebuilding
also requires the `base-devel` toolchain:

```sh
sudo pacman -S systemd-libs base-devel
```

## Interaction model

The selected input methods form a lazy priority queue. Rank one is always the
current Fcitx input method. Every other row has a single up-arrow that swaps it
with the row immediately above it.

```text
1. EN   keyboard-us     ACTIVE
2. VI   lotus               ↑
3. JA   mozc                ↑
```

Promoting rank three once changes only the ordering:

```text
1. EN   keyboard-us     ACTIVE
2. JA   mozc                ↑
3. VI   lotus               ↑
```

Promoting `mozc` again moves it to rank one and activates it through Fcitx:

```text
1. JA   mozc            ACTIVE
2. EN   keyboard-us         ↑
3. VI   lotus               ↑
```

The number of clicks required to activate a method is therefore exactly its
current rank minus one. The former head is demoted only to rank two; the queue
is never rotated to the tail.

Bar controls:

- Left click promotes rank two, providing fast switching between the two
  highest-priority methods.
- Right click opens the configuration panel.

Panel controls:

- `↑` promotes that method exactly one rank.
- `−` removes a non-active method from the queue.
- Clicking a configured method opens its label editor. The label accepts one
  to three characters and is normalized to uppercase.
- `Add input method` opens a picker containing methods from the active Fcitx
  group that are not already in the cycle. Selecting one appends it to the
  bottom of the queue.

New entries use Fcitx's language metadata as their initial label, such as
`EN`, `VI`, or `JA`. This is a language label, not an abbreviation of the
input-method name: Lotus therefore defaults to `VI`, not `LOT`. The label is
fully user-editable because one language may have several methods, and users
may prefer a regional or method-specific distinction.

Those codes are examples, not a built-in language table. The plugin reads the
language code reported by Fcitx when a method is first added, then persists
that initial value or the user's edited value in `shell.json`. Persisting the
label is necessary so a custom choice survives shell restarts; it does not
restrict which languages can be selected.

At least one method remains selected. The active method cannot be removed;
activate another method first. Changes are written immediately to the widget's
entry in `~/.config/omarchy/shell.json`.

If Fcitx is changed outside the widget, the reported active method is moved to
the head automatically. An externally activated method that was not selected
is admitted at the head so the queue continues to represent actual Fcitx
state.

## Architecture

The manifest declares both `bar-widget` and `service`. Quattro creates one bar
widget per monitor but only one service for the shell session:

```text
Fcitx Controller1 D-Bus interface
               ↓
    fcitx-state-monitor
               ↓
      Service.qml singleton
               ↓ shared state and commands
    BarWidget.qml per monitor
               ↓ right click
          Panel.qml
```

`Service.qml` owns the persistent observer and exposes the current method,
configured methods, refresh, and activation operations. Every bar instance
obtains the same service through:

```qml
bar.shell.serviceFor("nkarl.en-vi-indicator")
```

The priority queue remains in Quattro's inline widget settings, so all monitor
instances receive the same persisted ordering. Adding monitors does not add
observer processes or D-Bus clients.

### Native helper modes

Fcitx 5.1.21 exposes `CurrentInputMethod()` but no corresponding
`CurrentInputMethodChanged` signal. The installed Quickshell build also lacks
a general-purpose QML D-Bus client. A small C helper bridges that gap using
`sd-bus`.

With no arguments, it stays alive, checks the current method every 750
milliseconds, sleeps on the bus between checks, and emits only changes:

```sh
./fcitx-state-monitor
```

The same binary provides two short-lived commands used only during setup or a
user interaction:

```sh
# List methods in the active Fcitx group.
./fcitx-state-monitor --list

# Activate an exact Fcitx input-method ID.
./fcitx-state-monitor --set lotus
```

This keeps continuous observation efficient while avoiding a second protocol
or daemon. Rapid activation requests are queued by `Service.qml` rather than
dropped while a previous request exits.

If Fcitx is temporarily unavailable, the observer emits nothing and preserves
the last valid state. Later checks resume normally when Fcitx returns.

## Resource use

The singleton observer measured approximately:

```text
RSS:             3.0 MB
Private memory:  184 KB
PSS:             290 KB
Idle CPU:        0.0% at the sampled resolution
```

RSS includes shared `libsystemd`, `libc`, and loader pages, so private memory
and PSS better describe the incremental cost. The singleton service keeps this
cost constant as monitors are added.

## Files

- `manifest.json` declares the combined service and bar widget.
- `Service.qml` owns the singleton observer and Fcitx commands.
- `BarWidget.qml` owns the queue, persistence, and mouse interactions.
- `Panel.qml` provides the Quattro-styled queue editor.
- `fcitx-state-monitor.c` contains the helper source.
- `fcitx-state-monitor` is the compiled runtime helper.

## Build and reload

From the plugin directory:

```sh
cc -std=c17 -O2 -Wall -Wextra -pedantic -fPIE -pie \
  fcitx-state-monitor.c -o fcitx-state-monitor \
  $(pkg-config --cflags --libs libsystemd)
```

Validate the plugin after changes:

```sh
omarchy plugin validate nkarl.en-vi-indicator
```

Plugin source changes normally hot-reload. After changing a manifest, rescan
and then restart only if the running registry still reports stale metadata:

```sh
omarchy-shell shell rescanPlugins
omarchy restart shell
```

Do not run rescan and restart concurrently. Quickshell 0.3.0.r20 was observed
crashing in `IpcHandler::updateRegistration` when a plugin rescan overlapped a
shell restart. The replacement shell recovered automatically, but the two
lifecycle operations should remain sequential.

## Decision history

The original widget launched `fcitx5-remote -n` every 250 milliseconds, which
created four processes per second for every monitor. A SystemTray alternative
was also rejected because it inferred state from icon names and the live
StatusNotifier watcher had no registered Fcitx item.

The first persistent helper used Qt Core and Qt D-Bus and was instantiated by
each bar widget. Moving the observer into a Quattro service made it a
singleton, and rewriting it with `sd-bus` reduced its private footprint.

The widget initially disappeared after required QML imports were removed. A
later manifest update was masked by a visible backup directory containing the
same plugin ID. That backup is now `.nkarl.en-vi-indicator.bak`; Quattro ignores
hidden plugin directories. Do not keep visible backup plugin directories with
duplicate manifest IDs under `~/.config/omarchy/plugins/`.

## Diagnostics

```sh
# Registry should report both "bar-widget" and "service".
omarchy plugin list --json | jq '.[] | select(.id == "nkarl.en-vi-indicator")'

# Exactly one observer should exist regardless of display count.
pgrep -fc '^/home/kyle/.config/omarchy/plugins/nkarl.en-vi-indicator/fcitx-state-monitor$'

# Inspect the persisted queue.
jq '.bar.layout.right[] | select(.id == "nkarl.en-vi-indicator")' \
  ~/.config/omarchy/shell.json

# Inspect recent plugin errors.
journalctl --user --since '10 minutes ago' --no-pager | \
  rg 'nkarl.en-vi-indicator|Plugin widget.*failed|service plugin'
```
