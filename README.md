# rofi-menu

A command-line launcher that reads a nested JSON configuration file and
presents it as a cascading [rofi](https://github.com/davatorium/rofi) dmenu.
Each entry in the JSON can be a plain shell command, a command with metadata
(confirmation prompt, working directory, environment variables, custom theme),
or a sub-menu that opens a new rofi window.

## Requirements

| Dependency | Minimum version | Notes |
|---|---|---|
| `rofi` | 1.7+ | must be in `$PATH` |
| `json-c` | 0.15+ | header + shared library |
| `gcc` / `clang` | C11-capable | also needs `pkg-config` |
| `make` | any | for the provided Makefile |

On Arch Linux:
```bash
sudo pacman -S rofi json-c base-devel
```

## Build

```bash
# Release build
make

# Debug build (AddressSanitizer + UBSan, no optimisation)
make debug

# Clean build artefacts
make clean

# Install to /usr/local/bin
sudo make install
```

## Usage

```
rofi-menu [OPTIONS] <config.json>

Options:
  -t, --theme <path>   Path to a .rasi rofi theme file
  -i, --icons          Enable icons in rofi (default: on)
  -I, --no-icons       Disable icons in rofi
  -v, --verbose        Print info messages to stderr
  -d, --debug          Print debug messages to stderr (implies --verbose)
  -h, --help           Show help and exit
  -V, --version        Show version and exit

Exit codes:
  0  Success
  1  --help or --version printed
  2  Invalid arguments
  3  Runtime error (file not found, JSON parse error, …)
```

Example — bind to a key in your i3 config:
```
bindsym $mod+m exec rofi-menu ~/.config/rofi-menu/config.json
```

## Getting Started

Copy the example configuration and customise it for your system:
```bash
cp config.example.json config.json
$EDITOR config.json
```

`config.json` is listed in `.gitignore` so your personal configuration is
never committed. `config.example.json` is the template tracked by the repo.

## JSON Configuration

The top-level object is the root menu. Every key is a menu entry label.

### Simple command (string value)
```json
{
  "Terminal": "kitty",
  "Editor":   "nvim ~/notes"
}
```

### Command with metadata (object with `::cmd`)
```json
{
  "Deploy": {
    "::cmd":     "bash ~/scripts/deploy.sh",
    "::confirm": true,
    "::cwd":     "~/projects/myapp",
    "::env":     { "ENV": "production" }
  }
}
```

### Sub-menu (object without `::cmd`)
```json
{
  "Database": {
    "::prompt": "DB Tool",
    "DBeaver":  "dbeaver",
    "CLI":      "kitty -e mariadb -u root -p"
  }
}
```

### All supported metadata keys

| Key | Type | Description |
|---|---|---|
| `::cmd` | string | The shell command to execute |
| `::prompt` | string | Custom rofi `-p` prompt text |
| `::theme` | string | Path to a `.rasi` theme for this level |
| `::icons` | bool | Override global icon setting (`true`/`false`) |
| `::env` | object | Key-value pairs set as environment variables |
| `::cwd` | string | Working directory before running the command |
| `::confirm` | bool | Show a confirm/cancel prompt before executing |
| `::hidden` | bool | Hide this entry from the menu |

Paths in `::cwd` and `::theme` support `~` expansion.

### Example `config.json`

See [config.example.json](config.example.json) for a full working example.

## Project Structure

```
rofi-nested-menu-c/
├── Makefile
├── config.example.json  example configuration
├── include/
│   └── config.h         compile-time constants, macros, feature flags
└── src/
    ├── main.c            entry point: argument parsing, JSON loading, menu dispatch
    ├── args.c / args.h   command-line argument parsing (Config struct)
    ├── menu.c / menu.h   recursive JSON menu walker and command executor
    └── rofi.c / rofi.h   rofi process management and shell execution helpers
```

## Credits

Based on the original work by
[Christian Schweigel](https://github.com/cschweig2) — rewritten in C
with an extended feature set. Original license preserved in [LICENSE](LICENSE).

## License

MIT — see [LICENSE](LICENSE).
