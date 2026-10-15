# OmaFreewrite User Guide

OmaFreewrite is a writing app for getting words out of your head without
stopping to look at them. You type, the screen goes black except for one dim
line of what you just wrote, and when you come up for air an AI pass called
Polish quietly fixes the spelling and breaks the text into short paragraphs.
It never changes a word you wrote.

This guide covers everything in the app. The short version:

1. Launch OmaFreewrite and start typing.
2. After about fifteen words the screen fades to black. Keep going.
3. Press `Escape` when you are done with a burst. The page comes back and
   Polish cleans up the spelling in the background.
4. `Ctrl+S` saves. `Ctrl+Z` undoes a Polish pass if you ever want to.

---

## 1. Installing and launching

OmaFreewrite installs on Omarchy (Arch Linux) as a package and sits beside the
original Omawrite without sharing any settings.

```sh
cd ~/omafreewrite
./bin/install          # builds the package and installs it (asks for sudo)
```

Launch it from the app launcher as **OmaFreewrite**, or from a terminal:

```sh
omafreewrite                 # a blank document
omafreewrite notes.md        # open a file
```

For Polish to work out of the box you also need Ollama, the local model runner,
and one model. This is a one-time setup:

```sh
sudo pacman -S ollama
sudo systemctl enable --now ollama
ollama pull gemma3:4b
```

Polish also works with cloud models if you prefer. See section 4.

## 2. Writing

The page is plain Markdown. Type as you would anywhere. Headings, lists, bold
and links are rendered lightly in place so the file stays readable, but what
is on disk is always plain text you can open anywhere.

- Word count sits in the bottom right.
- `Ctrl+B`, `Ctrl+I` and `Ctrl+K` wrap the selection in bold, italic or a
  link. With a URL on the clipboard, `Ctrl+K` uses it.
- `Ctrl+F` finds, `Ctrl+H` finds and replaces. `Enter` or `Ctrl+G` jumps to the
  next match, `Shift+Enter` to the previous.
- The font button in the footer picks any installed font. IBM Plex Mono is the
  default and the choice is remembered.
- Text size follows the desktop text size (`omarchy display text size`), so you
  never need to zoom the app itself.

## 3. Focus mode

Focus mode is the heart of the app. It takes the page away so there is nothing
to re-read, nothing to fix, and no cursor to wander back with.

**How it starts.** Once you have typed fifteen new words, the window goes
fullscreen and fades to true black. A single dim line fades in near the top
showing the tail of what you are writing. New text pushes the old off to the
left like a ticker. It counts words you write, not words in the file, so
opening a long document never triggers it.

**What works inside it.** Letters, space, Enter and Backspace. That is all.
Arrow keys, Home, End, Page Up and Page Down and the mouse do nothing. Forward
only.

**How it ends.**

| Key | What happens |
|---|---|
| `Escape` | The page fades back and Polish runs on what you just wrote. |
| `Shift+Escape` | The page fades back and nothing else happens. |
| `Ctrl+Shift+F` | Toggles focus mode by hand, in or out, at any time. |

After you leave, the counter re-arms, so the next fifteen words bring it back.

**Adjusting it.**

- `Ctrl+=` and `Ctrl+-` change the size of the line while you are in the mode.
  The choice is remembered.
- `Ctrl+S` still saves while you are in it. If the document has never been
  saved, the app leaves focus mode and shows the file picker first.

Three settings live in `~/.config/Omacom/omafreewrite.conf`:

```ini
[focus]
lineFontSize=14   ; pixels, 9 to 40
wordThreshold=15  ; new words before the blackout
autoEnter=true    ; false leaves only the Ctrl+Shift+F toggle
```

## 4. Polish

Writing with your eyes off the screen leaves typos everywhere. Polish is the
cleanup. When `Escape` releases focus mode, the words from that session are
sent to a language model with one job:

1. Fix spelling, typos, capitalization and punctuation.
2. Break the text into short paragraphs. One complete thought per paragraph,
   never more than two sentences.

**It never changes your words.** Not a synonym, not a rephrase, not a word
added or taken away. This is enforced twice. The instructions to the model
forbid it, and the app compares the reply with what you wrote. If the words
drift beyond spelling-level fixes, the reply is thrown away and the footer
says `Polish failed: the model changed the words, so nothing was touched`.
Your text stays exactly as it was.

**What you see.**

- The footer shows `Polishing…` while it works and `Polished` when it lands.
  The sparkle button in the footer lights up in the accent colour meanwhile.
- It runs in the background. Keep writing. If you type inside the span it was
  working on before it returns, the result is dropped rather than applied, and
  the footer says `Polish skipped: the text moved on`.
- Only the words from that focus session are touched. Earlier paragraphs are
  never resent.
- `Ctrl+Z` undoes a whole pass in one step.

**Speed.** With the default local model on a laptop processor, a few hundred
words take roughly twenty to thirty seconds. A cloud model does it in two or
three. Since it runs in the background, the wait rarely gets in the way.

### Polish settings

Click the sparkle button in the footer or press `Ctrl+Shift+P`.

| Field | What it does |
|---|---|
| Switch | Turns Polish on or off. Off, `Escape` just leaves focus mode. |
| Server | Where to send the text. Default is Ollama on this machine. |
| Model | Pick from the list the server offers, or type a name. `Refresh` re-reads the list. |
| API key | Blank for Ollama. Required for cloud services. |

Settings are stored under `[polish]` in the config file. The API key is kept
in plain text there, so treat the file accordingly.

### Choosing a model

**Local (default).** Ollama runs on your machine. Nothing you write leaves the
computer, and there is no cost. The default model `gemma3:4b` is the most
careful about leaving words alone. To try a faster one:

```sh
ollama pull llama3.2:3b
```

then pick it from the model list. Smaller models are quicker but a little more
likely to get creative, in which case the word guard will reject the pass.

**Cloud.** Any service that speaks the OpenAI chat format works. Set the server
to its base URL and add your key, for example:

| Service | Server | Model to try |
|---|---|---|
| Anthropic | `https://api.anthropic.com` | `claude-haiku-5-5` |
| OpenAI | `https://api.openai.com/v1` | the cheapest current model |
| Groq | `https://api.groq.com/openai/v1` | any small Llama |
| OpenRouter | `https://openrouter.ai/api/v1` | anything inexpensive |

Polish is about the simplest task you can give a model, so the cheapest tier of
any service is plenty. A session of a few hundred words costs a tiny fraction
of a cent.

## 5. Saving, opening, recovery

- `Ctrl+S` saves. The first time, a file picker asks where. Files are plain
  `.md`.
- `Ctrl+Shift+S` saves under a new name. `Ctrl+O` opens. `Ctrl+N` opens a
  second window. `Ctrl+P` prints.
- The title bar shows `*` while there are unsaved changes.
- Closing with unsaved changes asks whether to save, discard, or cancel.
- If the app ever exits abnormally, your unsaved draft is recovered on the
  next launch. Note that choosing **Discard** when closing deletes that
  recovery copy on purpose.
- If another program changes a file you have open, the app warns you before
  anything is overwritten.

## 6. Keyboard reference

Press `Ctrl+?` inside the app to see this list.

| Keys | Action |
|---|---|
| `Ctrl+S` | Save |
| `Ctrl+Shift+S` | Save as |
| `Ctrl+O` | Open |
| `Ctrl+N` | New window |
| `Ctrl+P` | Print |
| `Ctrl+Z` / `Ctrl+Shift+Z` / `Ctrl+Y` | Undo / redo |
| `Ctrl+F` | Find |
| `Ctrl+H` | Find and replace |
| `Ctrl+B` / `Ctrl+I` / `Ctrl+K` | Bold / italic / link |
| `F11` or `Super+F` | Fullscreen |
| `Ctrl+Shift+F` | Toggle focus mode |
| `Escape` | Leave focus mode and Polish |
| `Shift+Escape` | Leave focus mode without Polish |
| `Ctrl+=` / `Ctrl+-` | Focus line size |
| `Ctrl+Shift+P` | Polish settings |
| `Ctrl+?` | Shortcut reference |

## 7. Troubleshooting

**The footer says `Polish failed: no server at http://127.0.0.1:11434`.**
Ollama is not running. Start it with `sudo systemctl start ollama`, or enable
it permanently with `sudo systemctl enable --now ollama`. If you started
`ollama serve` by hand in a terminal, that copy stops when the terminal
closes and keeps its models in a different place; use the service instead.

**The footer says `Polish failed: model not found` or similar.** The model
named in the settings is not pulled. Run `ollama pull gemma3:4b` (or whichever
you chose), then press `Refresh` in the Polish settings.

**The footer says `Polish failed: the model changed the words`.** The guard
did its job. Nothing was altered. Try again, or switch to `gemma3:4b` if you
were testing a smaller model.

**Polish is slow.** On a laptop processor that is normal for a local model.
Either accept the background wait, switch to a smaller model, or point the
settings at a cloud service.

**Nothing happens on `Escape`.** Check the switch in the Polish settings is on,
and that you wrote at least three words in the session. Very short bursts are
skipped on purpose.

**The app looked frozen after `Ctrl+S`.** This was a bug in versions before
0.2.0: the save dialog opened behind the fullscreen blackout. It is fixed. If
you are on an older build, run `./bin/install` again.

**I want Polish to do more, like fix grammar or tighten sentences.** It will
not, by design. The whole point is that what you said stays what you said.
Edit by hand afterwards, with the page in front of you.

## 8. Where things live

| What | Where |
|---|---|
| Settings | `~/.config/Omacom/omafreewrite.conf` |
| Draft recovery | `~/.local/share/Omacom/omafreewrite/` |
| Ollama models | `/var/lib/ollama` (owned by the service) |
| Source | `~/omafreewrite` |
