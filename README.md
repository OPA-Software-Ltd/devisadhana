```markdown
## Config file
You can keep your preferred settings in a file. CLI flags still override config.

**Default path:**
- `$XDG_CONFIG_HOME/sadhana/sadhana.toml` or
- `~/.config/sadhana/sadhana.toml`

**Use a custom path:**
```bash
./build/bin/sadhana --config ~/my-sadhana.toml
```

**Format (TOML/INI-like):**
```toml
# Choose device and audio params
device = 6
sample_rate = 48000
frames_per_buffer = 480

# Calibration and VAD tuning
calibrate_ms = 5000
calib_attack = 18
calib_rel_above_floor = 6
vad_hang_ms = 120
show_thresholds = true

# Optional: DB path (supports ~ expansion)
# db_path = "~/.local/share/sadhana/sadhana.db"
```

**Precedence:** `CLI` > `config` > defaults.

