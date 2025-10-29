# FlatBuffer Schemas

This directory contains FlatBuffer schema (`.fbs`) files that define the communication protocol with HyperHDR.

## Schema Files

- **hyperion_request.fbs** - Defines the request message structure sent to HyperHDR
- **hyperion_reply.fbs** - Defines the reply message structure received from HyperHDR

## Build Process

The C++ header files are **automatically generated** during the build process by CMake. You don't need to manually generate them.

### How It Works

1. When you run `cmake`, it finds the `flatc` compiler
2. During build, CMake automatically generates headers in `include/flatbuffer/`
3. Generated headers (`*_generated.h`) are excluded from git via `.gitignore`

### Requirements

- **FlatBuffers** package must be installed on your system
- The `flatc` compiler must be in your PATH

### Installation

#### macOS
```bash
brew install flatbuffers
```

#### Raspberry Pi / Debian / Ubuntu
```bash
sudo apt-get install flatbuffers-compiler libflatbuffers-dev
```

#### Other Systems
See [FlatBuffers documentation](https://google.github.io/flatbuffers/flatbuffers_guide_building.html)

## Modifying Schemas

If you need to modify the protocol:

1. Edit the `.fbs` files in this directory
2. Re-run cmake and rebuild - headers will be regenerated automatically
3. The schema files (`.fbs`) should be committed to git
4. The generated headers should NOT be committed (they're in `.gitignore`)

## Cross-Platform Compatibility

This setup ensures that:
- Each system generates headers compatible with its compiler and architecture
- No pre-built generated files are checked into version control
- The build process is reproducible across different platforms

