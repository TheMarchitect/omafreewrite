# OmaFreewrite

A focused Markdown writing app for Omarchy. Write a while and the whole screen
fades to black except one small line of what you just typed, in the spirit of
the Freewrite Alpha: no page to re-read, no cursor to wander, nothing to fix.
Press Escape and the page fades back.

OmaFreewrite is a fork of [Omawrite](https://github.com/omacom-io/omawrite),
the dead-simple Markdown writer built with Qt Quick and C++ that follows system
dark/light mode. It installs beside the original with its own settings, so the
two never interfere.

## Focus mode

- Once you have typed 15 new words, the window fades to black, goes fullscreen,
  and a single dim line fades in showing the tail of what you are writing. New
  text pushes the old off to the left like a ticker.
- The count is of words written, not words in the file, so opening a long
  document never triggers it. Deleting words lowers the count.
- Letters, space, Enter and Backspace work. Arrow keys, Home, End, Page keys and
  the mouse do nothing. Forward only.
- `Escape` releases it. The counter re-arms from there, so the next 15 words
  bring it back.
- `Ctrl+Shift+F` toggles it by hand at any time.
- `Ctrl+=` and `Ctrl+-` change the line's text size while in the mode, and the
  choice is remembered.
- `Ctrl+S` keeps working the whole time. If it needs the save dialog, focus
  mode and fullscreen drop first so the portal's picker is not hidden behind
  the blackout.

The screen is always true black, whatever the system theme.

## Polish

Freewriting with your eyes off the screen leaves typos everywhere. When Escape
releases focus mode, Polish sends the words from that session to a language
model and asks for exactly three things: spelling, capitalization and
punctuation fixed, and the text broken into short paragraphs of one complete
thought and at most two sentences.

The words themselves are never changed. The prompt forbids rewording, adding,
removing or reordering, and the reply is checked against what you wrote: if the
word sequence drifts beyond spelling-level edits, the pass is discarded and the
footer says so. The two-sentence cap and straight quotes are enforced in code
after the model answers, so a small model does not need to be perfect.

- It runs in the background after the fade, and the footer shows `Polishing…`
  then `Polished`. Keep typing; if the span changed meanwhile, the result is
  dropped rather than applied.
- `Ctrl+Z` undoes a pass in one step.
- `Shift+Escape` leaves focus mode without a pass.
- The sparkle button in the footer (or `Ctrl+Shift+P`) opens the settings:
  the server, the model, an optional API key, and the on/off switch.

By default it talks to [Ollama](https://ollama.com) on this machine with
`gemma3:4b`, so nothing leaves your computer:

```sh
sudo pacman -S ollama
sudo systemctl enable --now ollama
ollama pull gemma3:4b
```

Any OpenAI-compatible chat server works the same way (set the server to its
base URL, such as `https://api.groq.com/openai/v1`, and add the key). For the
Anthropic API use `https://api.anthropic.com`, a key, and a model such as
`claude-haiku-5-5`. Settings live under `[polish]` in the config file:

```ini
[polish]
enabled=true
baseUrl=http://127.0.0.1:11434
model=gemma3:4b
apiKey=            ; stored in plain text; leave blank for Ollama
```

On a laptop CPU, a 4B model polishes a few hundred words in roughly twenty to
thirty seconds. A cloud model does it in a couple of seconds.

Three focus settings live in `~/.config/Omacom/omafreewrite.conf` under `[focus]`:

```ini
[focus]
lineFontSize=14   ; pixels, 9 to 40
wordThreshold=15  ; new words before the blackout
autoEnter=true    ; false leaves only the Ctrl+Shift+F toggle
```

## Build and install

```sh
./bin/build      # builds build/omafreewrite
./bin/test       # runs the test suite offscreen
./bin/install    # builds an Arch package and installs it with makepkg
```

## Shortcuts

- `Ctrl+S` saves. Unsaved documents use the XDG desktop portal file picker.
- `Ctrl+Shift+S` saves as.
- `Ctrl+O` opens a Markdown file through the portal picker.
- `Ctrl+P` opens the system print dialog.
- `Ctrl+N` opens a new OmaFreewrite window.
- `Ctrl+Z`, `Ctrl+Shift+Z`, and `Ctrl+Y` handle undo and redo.
- `Super+F` toggles fullscreen. Qt maps this key as `Meta+F`.
- `Ctrl+F` searches the document. Use `Enter` or `Ctrl+G` for the next match and `Shift+Enter` for the previous match.
- `Ctrl+H` opens find and replace.
- `Ctrl+B`, `Ctrl+I`, and `Ctrl+K` insert bold, italic, and link Markdown.
- `Ctrl+Shift+F` toggles focus mode. `Escape` leaves it and runs Polish; `Shift+Escape` leaves it without.
- `Ctrl+Shift+P` opens the Polish settings.
- `Ctrl+?` shows the keyboard shortcut reference.

Unsaved drafts are recovered after an abnormal exit. OmaFreewrite also watches open files
and warns before an external change can replace local work.

The font button in the footer picks the writing font from any installed text font,
and OmaFreewrite remembers the choice. IBM Plex Mono is the default.

Text follows the desktop text size — `omarchy display text size`, or GNOME's
`text-scaling-factor` — and re-flows without a restart. The default of 12px leaves
OmaFreewrite at the size it is designed around; larger and smaller sizes scale from there.

## Requirements

- Qt 6: `qt6-base`, `qt6-declarative`, `qt6-quickcontrols2`
- `xdg-desktop-portal` and a portal backend

The IBM Plex Mono font is bundled under the SIL Open Font License 1.1; see
`fonts/OFL.txt`. The font is copyright IBM Corp.

## License

MIT, as Omawrite is. See `LICENSE`.
