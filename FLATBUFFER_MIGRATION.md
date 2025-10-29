# FlatBuffer Build System Migration

## Summary of Changes

This document describes the migration to automatic FlatBuffer header generation during the build process, ensuring cross-platform compatibility.

## What Changed

### 1. Schema Files Moved
- **Before**: Schema files (`.fbs`) were in `include/flatbuffer/`
- **After**: Schema files moved to dedicated `schemas/` directory
- **Why**: Clearer separation between source schemas and generated code

### 2. Generated Headers Excluded from Git
- **Before**: Generated headers (`*_generated.h`) were checked into version control
- **After**: Generated headers are excluded via `.gitignore` and built automatically
- **Why**: Generated code should not be in version control; each system generates headers compatible with its compiler/architecture

### 3. CMake Auto-Generation
- **Before**: Headers had to be manually generated with `flatc`
- **After**: CMake automatically generates headers during build process
- **Why**: Ensures headers are always up-to-date and compatible with the build system

## Directory Structure

```
cpp/
├── schemas/                          # Source schemas (checked into git)
│   ├── README.md                     # Documentation
│   ├── hyperion_reply.fbs           # Reply message schema
│   └── hyperion_request.fbs         # Request message schema
│
└── include/flatbuffer/               # Generated headers (NOT in git)
    ├── hyperion_reply_generated.h   # Auto-generated during build
    └── hyperion_request_generated.h # Auto-generated during build
```

## Build Process

### Step 1: CMake Configuration
When you run `cmake ..`, it will:
1. Find the `flatc` compiler on your system
2. Locate schema files in `schemas/` directory
3. Set up build rules to generate headers

### Step 2: Header Generation
During `make`, before compiling any source files:
1. CMake runs `flatc` for each schema file
2. Generates `*_generated.h` files in `include/flatbuffer/`
3. Creates the `generate_flatbuffers` target

### Step 3: Compilation
After headers are generated:
1. Source files compile normally
2. They include the newly generated headers
3. Everything links together as before

## For Developers

### First-Time Setup
```bash
# Install FlatBuffers compiler
# macOS:
brew install flatbuffers

# Raspberry Pi / Debian / Ubuntu:
sudo apt install flatbuffers-compiler libflatbuffers-dev

# Build project
mkdir build && cd build
cmake ..
make
```

### Clean Builds
```bash
# Remove all build artifacts including generated headers
rm -rf build include/flatbuffer/*_generated.h

# Rebuild from scratch
mkdir build && cd build
cmake ..
make
```

### Modifying Schemas
```bash
# 1. Edit schema file
vi schemas/hyperion_request.fbs

# 2. Rebuild (headers auto-regenerate)
cd build
make

# 3. Commit schema changes (NOT generated headers)
git add schemas/hyperion_request.fbs
git commit -m "Update HyperHDR request schema"
```

## Git Workflow

### What to Commit
✅ **DO commit**:
- Schema files (`schemas/*.fbs`)
- CMakeLists.txt changes
- Documentation updates

❌ **DON'T commit**:
- Generated headers (`include/flatbuffer/*_generated.h`)
- Build directories (`build/`)

### .gitignore Entry
```gitignore
# Generated flatbuffer headers
include/flatbuffer/*_generated.h
```

## Cross-Platform Compatibility

### Why This Matters
Different systems may generate slightly different headers due to:
- Different compiler versions
- Different architectures (ARM vs x86)
- Different FlatBuffers versions
- Different OS conventions

By generating headers at build time, we ensure:
- ✅ Each system uses compatible headers
- ✅ No merge conflicts on generated code
- ✅ Always up-to-date with schema changes
- ✅ Reproducible builds across platforms

### Platform-Specific Notes

#### macOS (Development)
```bash
# Install via Homebrew
brew install flatbuffers

# Build uses clang/AppleClang
cmake .. && make
```

#### Raspberry Pi (Production)
```bash
# Install via apt
sudo apt install flatbuffers-compiler libflatbuffers-dev

# Build uses gcc/g++
cmake .. && make -j4
```

#### CI/CD Systems
```yaml
# Example: GitHub Actions
- name: Install FlatBuffers
  run: |
    sudo apt update
    sudo apt install -y flatbuffers-compiler libflatbuffers-dev
    
- name: Build
  run: |
    mkdir build && cd build
    cmake ..
    make
```

## Troubleshooting

### Error: FlatBuffers package not found
```
CMake Error: Could NOT find package "Flatbuffers"
```

**Cause**: FlatBuffers package name is case-sensitive and varies by system:
- Some systems: `flatbuffers` (lowercase)
- Other systems: `Flatbuffers` (capitalized)

**Solution**: The updated CMakeLists.txt now tries both variants automatically. If you still see this error:

```bash
# Check if FlatBuffers is installed
flatc --version

# If not found, install it:
# macOS:
brew install flatbuffers

# Raspberry Pi / Debian / Ubuntu:
sudo apt install flatbuffers-compiler libflatbuffers-dev
```

### Error: Generated header not found during compilation
```
fatal error: flatbuffer/hyperion_request_generated.h: No such file or directory
```

**Cause**: Headers weren't generated before compilation started.

**Solution**: Headers are now generated during CMake configuration phase, not build phase. This ensures they exist before any source files try to include them.

### Error: FlatBuffers not available on system
If FlatBuffers is not installed, the build will continue but HyperHDR communication will be disabled:

```bash
# Build will show warnings but succeed:
-- FlatBuffers not found. HyperHDR communication will be disabled.
-- Install with: sudo apt install flatbuffers-compiler libflatbuffers-dev
```

The application will compile and run, but HyperHDR features will be disabled at runtime.

### Error: flatc not found
```
CMake Warning: flatc compiler not found
```

**Solution**: Install FlatBuffers compiler (see Prerequisites in README.md)

### Error: Generated header out of date
If you see compilation errors after modifying a schema:

```bash
# Force regenerate headers
cd build
make clean
make generate_flatbuffers
make
```

### Error: Headers not generated
If headers aren't in `include/flatbuffer/` after build:

```bash
# Check CMake found flatc
cd build
cmake .. | grep flatc

# Manually run generation target
make generate_flatbuffers

# Check output
ls -la ../include/flatbuffer/
```

## Migration Steps (For Other Projects)

If you want to apply this pattern to other projects:

1. Create `schemas/` directory
2. Move `.fbs` files to `schemas/`
3. Add to `.gitignore`: `include/flatbuffer/*_generated.h`
4. Update CMakeLists.txt with generation rules:
   ```cmake
   find_program(FLATC_EXECUTABLE NAMES flatc REQUIRED)
   add_custom_command(OUTPUT ${GENERATED_HEADER}
       COMMAND ${FLATC_EXECUTABLE} --cpp -o ${OUTPUT_DIR} ${SCHEMA}
       DEPENDS ${SCHEMA})
   add_custom_target(generate_flatbuffers DEPENDS ${GENERATED_HEADERS})
   add_dependencies(your_target generate_flatbuffers)
   ```
5. Remove generated headers from git: `git rm --cached include/flatbuffer/*_generated.h`
6. Commit changes

## Benefits

✅ **No manual header generation required**  
✅ **Cross-platform compatibility guaranteed**  
✅ **Cleaner version control**  
✅ **Always up-to-date with schemas**  
✅ **Reproducible builds**  
✅ **No merge conflicts on generated code**  

## References

- FlatBuffers documentation: https://google.github.io/flatbuffers/
- Schema files: `schemas/README.md`
- Build configuration: `CMakeLists.txt` (lines 86-129)

