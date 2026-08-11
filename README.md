# BITSTREAM-DECODER

Reverse-engineering project for the GTA SA-MP `.ter` BitStream recording format.

## Current objective

Recover the `.ter` container exactly, then decode each stored RakNet/SA-MP BitStream payload into packet/RPC fields.

## Confirmed `.ter` container

The recorder is a 32-bit Windows program, so the serialized `size_t` fields are 4 bytes. The record layout is:

```text
uint32  nameLen
byte[]  name
uint32  dateLen
byte[]  date
uint64  totalDuration
uint32  recordCount

repeat recordCount:
    uint64  timestamp
    uint8   isRPC
    int32   id
    int32   reliability
    uint32  dataSize
    byte[]  data
```

The sample `PASARMODERN_AYAM.ter` is 2410 bytes and contains 38 records. Parsing with the above layout terminates exactly at EOF.

## BitStream recording semantics

The supplied recorder resets the captured BitStream read pointer before recording it, then copies the complete used bit range. The writer stores:

```text
dataSize = (numberOfBitsUsed + 7) >> 3
```

and copies the BitStream data without an additional right-alignment operation. The loader reconstructs the payload with `WriteBits(data, dataSize * 8, false)`.

For packet records, the packet ID is present in the captured BitStream. For RPC records, the RPC ID is stored in the record metadata and is not prefixed to the RPC payload.

## RakNet BitStream reader

`src/bitstream_reader.hpp` now implements the relevant RakNet bit semantics needed by the recorder payloads:

- first logical bit is the MSB of the backing byte;
- `ReadBits(..., true)` right-aligns partial final bytes;
- native multi-byte values are interpreted in the original little-endian Windows environment;
- arbitrary bit positions can be read without requiring byte alignment;
- bounds checks reject reads beyond the recorded bit range.

This is intentionally a small reader rather than a reimplementation of the entire RakNet `BitStream` class. Additional RakNet operations such as compressed integers, normalized vectors/quaternions, and variable-length encodings will be added only when a confirmed SA-MP packet/RPC requires them.

## Confirmed Packet 207 / ID_PLAYER_SYNC

`PASARMODERN_AYAM.ter` contains 12 records with packet ID 207 and `dataSize == 69` (552 bits). The payload schema matches SA-MP's incoming `ID_PLAYER_SYNC` layout exactly:

```text
uint8   packetId
uint16  lrKey
uint16  udKey
uint16  keys
float3  position
float4  quaternion (w, x, y, z)
uint8   health
uint8   armour
2 bits  additionalKey
6 bits  weaponId
uint8   specialAction
float3  velocity
float3  surfingOffsets
uint16  surfingVehicleId
int16   animationId
int16   animationFlags
```

The total is exactly 552 bits / 69 bytes. There is no player ID field in this packet; the player identity is supplied by the surrounding RakNet/server context.

`src/packet_decoder.hpp` now contains a dedicated 552-bit decoder for ID 207. `ter_parser.exe` automatically applies it to records where `isRPC == 0`, `id == 207`, and `dataSize == 69`, while retaining the raw hex dump.

For the sample 207 records, the previously confirmed common state is:

```text
position         = (1868.021606, 2106.042725, 11.088171)
quaternion       = (0.975472, -0.0, 0.0, -0.220125)
health           = 99
armour           = 0
additionalKey    = 0
weaponId         = 0
specialAction    = 0
velocity         = (0, 0, 0)
surfingOffsets   = (0, 0, 0)
surfingVehicleId = 0
animationId      = 1189
animationFlags   = -32764
```

One 207 record has the packed additional-key/weapon byte equal to `0x40`, demonstrating that the byte is not simply padding; the lower 2 bits represent the 2-bit additional-key field and the upper 6 bits represent the weapon field under the SA-MP schema.

The standard schema is independently documented by Pawn.RakNet's `OnFootSync` reference and the SA-MP packet list. The 69-byte length also matches the exact BitStream size observed in the `.ter` sample.

## Current reverse-engineering status

```text
TER container                    CONFIRMED
Writer/loader relationship       CONFIRMED
BitStream copy semantics         CONFIRMED
RakNet bit reader                IMPLEMENTED
Packet metadata                  CONFIRMED
RPC metadata                     CONFIRMED
ID_PLAYER_SYNC (207)             CONFIRMED + DECODER
207 exact 552-bit consumption    CONFIRMED + DECODER
203 / AIM_SYNC                   NEXT
205                             NEXT
RPC 62                          NEXT
```

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
