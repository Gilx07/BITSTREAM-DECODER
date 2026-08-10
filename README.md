# BITSTREAM-DECODER

Reverse-engineering project for the GTA SA-MP `.ter` BitStream recording format.

## Current objective

Recover the `.ter` container exactly, then decode each stored RakNet/SA-MP BitStream payload into packet/RPC fields.

## Confirmed container layout

The recorder source supplied for this project writes the following fields in order:

```text
uint32  nameLen          // size_t on the 32-bit recorder
byte[]  name
uint32  dateLen          // size_t on the 32-bit recorder
byte[]  date
uint64  totalDuration    // type still tied to the recorder's declaration
uint32  recordCount      // size_t on the 32-bit recorder

repeat recordCount:
    uint64  timestamp    // based on current sample analysis
    bool    isRPC        // expected 1 byte on MSVC; must be validated
    byte    rpcId        // current working hypothesis; must be validated against EOF
    int32   reliability
    uint32  dataSize     // size_t on the 32-bit recorder
    byte[]  data
```

The sample `PASARMODERN_AYAM.ter` confirms the beginning of the container:

- name: `PASARMODERN_AYAM`
- date: `2026-08-06 14:18:49`
- totalDuration: `7157`
- recordCount: `38`

## Important distinction

The `.ter` file is a recording container. Its `data` fields are the bytes captured from a RakNet `BitStream`; they are not a separate encrypted `.ter` encoding. The supplied recorder reconstructs a `BitStream` with `WriteBits(data, dataSize * 8, false)` when loading.

The next validation step is to prove the record layout through the entire file and confirm that the final record ends exactly at EOF.

## Build

```text
cmake -S . -B build
cmake --build build --config Release
```

Run:

```text
build\\Release\\ter_parser.exe PASARMODERN_AYAM.ter
```

On a single-configuration generator the executable is normally under `build/ter_parser`.
