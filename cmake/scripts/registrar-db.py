#!/usr/bin/env python3
"""Convert the registrar's file-type database between its binary form and JSON.

The database (~/Settings/registrar) is an os::Settings file: a 16-byte header
("SlbS", header size, version, data size) and a flattened os::Message.  A
flattened message is its size, six 32-bit header words, then one block per
field: a 28-byte DataArray_s header (next, max size, current size, chunk size,
count, type code, name length), the name, and the values (fixed-size chunks,
or for variable-size types an int32 length before each value).  The registrar
keeps each file type as a nested message (T_MESSAGE) in the "type" array.

    registrar-db.py dump BINARY > types.json
    registrar-db.py build types.json BINARY

JSON values: strings as text (without the terminating NUL), int32 as numbers,
nested messages as objects, anything else as {"raw": hex}.  Field order and
type codes are kept, so build(dump(x)) == x.
"""
import json
import struct
import sys

T_INT32 = 0x04
T_BOOL = 0x07       # not used here, kept raw
T_STRING = 0x09     # os::T_STRING
T_MESSAGE = 0x0c

SETTINGS_MAGIC = b"SlbS"


def parse_msg(b):
    size = struct.unpack_from("<I", b, 0)[0]
    hdr = b[4:28].hex()
    fields, pos = [], 28
    while pos < size:
        _, _, cur, chunk, count, typ = struct.unpack_from("<6i", b, pos)
        nl = b[pos + 24]
        name = b[pos + 28:pos + 28 + nl].decode("latin-1")
        p, vals = pos + 28 + nl, []
        for _ in range(count):
            if chunk:
                v, p = b[p:p + chunk], p + chunk
            else:
                n = struct.unpack_from("<i", b, p)[0]
                v, p = b[p + 4:p + 4 + n], p + 4 + n
            vals.append(decode(typ, v))
        fields.append({"name": name, "type": typ, "fixed": bool(chunk), "values": vals})
        pos += cur
    return {"header": hdr, "fields": fields}


def decode(typ, v):
    if typ == T_MESSAGE:
        return parse_msg(v)
    if typ == T_STRING and v.endswith(b"\0"):
        return v[:-1].decode("latin-1")
    if typ == T_INT32 and len(v) == 4:
        return struct.unpack("<i", v)[0]
    return {"raw": v.hex()}


def encode(typ, v):
    if isinstance(v, dict) and "fields" in v:
        return build_msg(v)
    if isinstance(v, str):
        return v.encode("latin-1") + b"\0"
    if isinstance(v, int):
        return struct.pack("<i", v)
    return bytes.fromhex(v["raw"])


def build_msg(m):
    out = b""
    for f in m["fields"]:
        name = f["name"].encode("latin-1")
        data = [encode(f["type"], v) for v in f["values"]]
        if f["fixed"]:
            chunk, body = len(data[0]), b"".join(data)
        else:
            chunk, body = 0, b"".join(struct.pack("<i", len(d)) + d for d in data)
        cur = 28 + len(name) + len(body)
        out += struct.pack("<6iB3x", 0, cur, cur, chunk, len(data), f["type"], len(name)) + name + body
    return struct.pack("<I", 28 + len(out)) + bytes.fromhex(m["header"]) + out


def main():
    if sys.argv[1] == "dump":
        b = open(sys.argv[2], "rb").read()
        assert b[:4] == SETTINGS_MAGIC
        hs = struct.unpack_from("<I", b, 4)[0]
        json.dump({"settings_header": b[:hs].hex(), "message": parse_msg(b[hs:])},
                  sys.stdout, indent=1, ensure_ascii=False)
        print()
    elif sys.argv[1] == "build":
        j = json.load(open(sys.argv[2]))
        msg = build_msg(j["message"])
        hdr = bytearray(bytes.fromhex(j["settings_header"]))
        struct.pack_into("<I", hdr, 12, len(msg))          # data size
        open(sys.argv[3], "wb").write(bytes(hdr) + msg)
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
