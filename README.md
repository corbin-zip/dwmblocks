# Corbin's dwmblocks

Modular status bar for dwm, written in C. Each block runs a shell command and the joined output becomes the bar text. Forked from Luke Smith's build with a round of hardening and updates from upstream on top.

My block scripts (`sb-*`) live in [my dotfiles repo](https://github.com/corbin-zip/dotfiles/tree/master/.local/bin/statusbar), not here. If you want the bar as I run it, put those in your `$PATH`.

## What's different in this build

Stock dwmblocks is fragile in a few ways that only show up after running it for a while. These fixes were hunted down and implemented with Claude Code running Fable 5:

- block commands that hang are killed after `CMDTIMEOUT` (15 seconds) and the block keeps its previous text, so one stuck `curl` can't freeze the whole bar
- the signalfd and blocked signal mask are no longer leaked into block commands, so the `sb-*` scripts (and anything they spawn) run with a clean signal environment
- a stray `EINTR` from `poll` no longer kills the status loop
- click events that don't land on a block are ignored instead of misfiring on the wrong one
- fixed buffer math for blocks with icons and multi-character delimiters

## Modifying blocks

Blocks are defined in `config.h` as `{icon, command, interval, signal}` and compiled in. Edit, rebuild, restart:

```sh
sudo make install
killall -q dwmblocks; setsid dwmblocks &
```

## Signaling changes

A block with `interval > 0` reruns every N seconds. A block with `interval == 0` only updates when it receives its signal, which is an ideal choice for anything event-driven: rather than polling idly, update the block exactly when something changes.

For example, the volume module has signal 10, so `pkill -RTMIN+10 dwmblocks` refreshes it. My volume module never updates on its own; the volume keybindings in dwm send that signal after changing the volume.

Every block needs a unique signal number.

## Clickable modules

With the [statuscmd patch](https://dwm.suckless.org/patches/statuscmd/) applied in dwm (it is in [my build](https://github.com/corbin-zip/dwm)), clicking a block reruns its command with `$BLOCK_BUTTON` set to the mouse button. The `sb-*` scripts read it to open popups, toggle things, and so on. The statuscmd diffs are kept in `patches/` for reference; they're already applied to this tree. Credit for those patches goes to Daniel Bylinka (daniel.bylinka@gmail.com).
