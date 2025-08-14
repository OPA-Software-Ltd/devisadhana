```markdown
# Sadhana (Milestone 1: VAD + SQLite logger)

This version captures microphone input, runs a simple energy-based VAD, and logs sessions and VAD events into a SQLite database.

## Build

```bash
cmake -S . -B build -G Ninja
cmake --build build
./build/bin/sadhana --list-devices
./build/bin/sadhana -d <index> --sr 48000 --fpb 480
```

## Options
- `-l, --list-devices` – list available input devices
- `-d, --device <index>` – select specific input device
- `--sr <Hz>` – set sample rate (default 48000)
- `--fpb <frames>` – frames per buffer (default 480)
- `--db <path>` – SQLite DB path (default `$XDG_DATA_HOME/sadhana/sadhana.db`)

## Usage
When running, it prints `[SPEECH]` or `[silence]` with current RMS and dBFS. Transitions are recorded in `vad_events`, sessions in `sessions`.

Check the DB:
```bash
sqlite3 ~/.local/share/sadhana/sadhana.db 'SELECT * FROM sessions;'
sqlite3 ~/.local/share/sadhana/sadhana.db 'SELECT * FROM vad_events ORDER BY id DESC LIMIT 10;'
```

## Next steps
- Replace energy-based VAD with WebRTC VAD
- Add TUI timeline view
- Integrate keyword spotting (iteration markers) with Vosk/whisper.cpp
```
