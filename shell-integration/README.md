# Konsole shell integration: xdg-activation tokens

On Wayland, a GUI application launched from an interactive shell normally
cannot take keyboard focus: it has no `XDG_ACTIVATION_TOKEN`, so the compositor
applies focus-stealing prevention and the window opens unfocused.

These scripts close that gap. At each command, the shell asks Konsole (over the
private `ActivationToken` OSC) for a fresh activation token. Konsole resolves
the command to an installed application via its `.desktop` files and, only for
GUI apps, mints a token bound to the keypress that started the command and
sends it back. The shell exports it as `XDG_ACTIVATION_TOKEN` so the launched
app can activate its window.

## Usage

Source the file for your shell from your shell rc file:

```sh
# ~/.bashrc
. /usr/share/konsole/shell-integration/konsole-activation-token.bash
```

It is safe to source unconditionally: it probes for terminal support once and
does nothing in terminals that do not implement the OSC.

## Cost

The gating (is this a GUI app?) lives in Konsole, not the shell, so the shell
does not scan `.desktop` files. To keep non-GUI commands cheap, the client
remembers commands that are not GUI apps and skips them: `ls`, `cd`, `git`,
... do one terminal round-trip the first time they are seen and none after.
GUI launches request a fresh token each time, because activation tokens are
single-use.

## Protocol

```
query:  OSC 6969 ; ? ; <command> ST
reply:  OSC 6969 ; <token> ST       (empty token if <command> is not a GUI app)
```

`ST` is the string terminator `ESC \`. The OSC number `6969` is a placeholder
pending an agreed value.
