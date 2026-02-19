# HandleEnum

A Windows command-line tool that enumerates and filters all open system handles using the NT API (`NtQuerySystemInformation`). Useful for security research, debugging, and understanding which processes hold handles to specific kernel objects.

## Features

- Enumerate all open handles across every running process
- Filter by process ID, process name, handle type, or object name
- Sort output by PID, handle type, or object name
- Show aggregate match counts without full output (`--count`)
- Verbose diagnostic mode
- Automatically attempts to acquire `SeDebugPrivilege` for broader access

## Requirements

| Requirement | Version |
|---|---|
| OS | Windows (64-bit) |
| Compiler | GCC 14+ via [MSYS2 MinGW-w64](https://www.msys2.org/) |
| Build system | CMake 3.20+, Ninja |
| C++ standard | C++23 |

> **Note:** Administrative privileges are recommended. Without them, `SeDebugPrivilege` cannot be acquired and many handles belonging to other processes will be inaccessible.

## Building

```bat
cmake --preset mingw-ninja
cmake --build --preset build-mingw-ninja
```

The compiled binary is placed in `build/HandleEnum.exe`.

### Running Tests

```bat
cd build
ctest --output-on-failure
```

## Usage

```
HandleEnum.exe [OPTIONS]
```

### Options

| Flag | Long form | Argument | Description |
|---|---|---|---|
| `-p` | `--pid` | `<PID>` | Filter by process ID |
| `-n` | `--name` | `<ProcessName>` | Filter by process name (substring) |
| `-t` | `--type` | `<HandleType>` | Filter by handle type (e.g. `File`, `Event`) |
| `-o` | `--object` | `<ObjectName>` | Filter by object name (substring) |
| `-s` | `--sort` | `pid&#124;type&#124;name` | Sort output (default: `pid`) |
| `-c` | `--count` | — | Print only the count of matching handles |
| `-v` | `--verbose` | — | Print additional diagnostics |
| `-h` | `--help` | — | Display help message and exit |

## Examples

List all open handles on the system (requires elevation):

```bat
HandleEnum.exe
```

Show all handles owned by process 1234:

```bat
HandleEnum.exe --pid 1234
```

Show all `File` handles sorted by object name:

```bat
HandleEnum.exe --type File --sort name
```

Find all handles whose object name contains `\Device\HarddiskVolume`:

```bat
HandleEnum.exe --object \Device\HarddiskVolume
```

Count how many handles `notepad.exe` currently has open:

```bat
HandleEnum.exe --name notepad.exe --count
```

## Output Format

```
PID      Process         Type                     Name
4        System          File                     \Device\HarddiskVolume3\Windows\...
1234     notepad.exe     File                     \Device\HarddiskVolume3\Users\...
...
Matching handles: 42
```

Columns:

| Column | Description |
|---|---|
| `PID` | Owning process ID |
| `Process` | Owning process name (e.g. `explorer.exe`) |
| `Type` | Kernel object type (e.g. `File`, `Event`, `Mutant`) |
| `Name` | NT object name, or `N/A` if not available |

## Project Structure

```
HandleEnum/
├── include/
│   ├── app.hpp          # HandleEnumApp class (application entry point)
│   ├── cli_parser.hpp   # Command-line parsing interface
│   ├── filters.hpp      # IHandleFilter and concrete filter classes
│   ├── nt.hpp           # NT API wrappers (query handles, privilege, names)
│   ├── string_utils.hpp # String utility helpers
│   └── types.hpp        # Shared types: CliOptions, HandleInfo, SortField
├── src/
│   ├── app.cpp          # Application pipeline (filter, map, sort, print)
│   ├── cli_parser.cpp   # CLI argument parsing implementation
│   ├── filters.cpp      # Filter implementations (PID, type, name)
│   ├── main.cpp         # Entry point
│   ├── nt_query.cpp     # NtQueryObject wrappers (type and name)
│   ├── nt_system.cpp    # NtQuerySystemInformation + privilege helpers
│   └── string_utils.cpp # String utility implementations
├── tests/
│   ├── app_tests.cpp
│   ├── cli_parser_tests.cpp
│   ├── filters_tests.cpp
│   └── nt_tests.cpp
├── CMakeLists.txt
└── CMakePresets.json
```

## License

This project does not currently include a license file.
