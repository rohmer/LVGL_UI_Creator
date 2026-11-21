"""Build a flash lookup index for a compact NGTR v3 trie."""

from __future__ import annotations

import argparse
from array import array
from pathlib import Path
from struct import Struct


HEADER = Struct("<4sIII")
CHECKPOINT = Struct("<III")
NODES_PER_CHECKPOINT = 32
UINT32_MAX = (1 << 32) - 1


def read_var_uint32(data: bytes, offset: int) -> tuple[int, int]:
    value = 0
    for shift in range(0, 32, 7):
        if offset == len(data):
            raise ValueError("truncated NGTR record")
        byte = data[offset]
        offset += 1
        if shift == 28 and byte > 0x0F:
            raise ValueError("NGTR variable-length integer exceeds 32 bits")
        value |= (byte & 0x7F) << shift
        if byte < 0x80:
            if shift != 0 and byte == 0:
                raise ValueError("noncanonical NGTR variable-length integer")
            return value, offset
    raise ValueError("invalid NGTR variable-length integer")


def append_var_uint32(output: bytearray, value: int) -> None:
    while value >= 0x80:
        output.append((value & 0x7F) | 0x80)
        value >>= 7
    output.append(value)


def build_index(data: bytes) -> bytearray:
    if len(data) < HEADER.size or len(data) > UINT32_MAX:
        raise ValueError("invalid NGTR file length")
    magic, version, node_count, record_size = HEADER.unpack_from(data)
    if magic != b"NGTR" or version != 3 or record_size != 0:
        raise ValueError("expected a compact NGTR v3 model")
    if node_count < 2 or not 2 * node_count <= len(data) - HEADER.size <= 8 * node_count:
        raise ValueError("NGTR file length does not match the node count")

    checkpoint_count = (node_count + NODES_PER_CHECKPOINT - 1) // NODES_PER_CHECKPOINT
    mass_start = HEADER.size + checkpoint_count * CHECKPOINT.size
    output = bytearray(mass_start)
    HEADER.pack_into(output, 0, b"NGIX", 1, len(data), node_count)

    child_starts = array("I", [0]) * node_count
    masses = array("I", [0]) * node_count
    child_counts = bytearray(node_count)
    labels = bytearray(node_count)
    offset = HEADER.size
    next_child = 2
    for node in range(node_count):
        if node % NODES_PER_CHECKPOINT == 0:
            cp = HEADER.size + node // NODES_PER_CHECKPOINT * CHECKPOINT.size
            CHECKPOINT.pack_into(output, cp, offset, next_child, 0)
        if offset == len(data):
            raise ValueError("truncated NGTR record")
        label = data[offset]
        flags, offset = read_var_uint32(data, offset + 1)
        degree = flags >> 1
        if node >= next_child or degree > 255 or next_child + degree > node_count:
            raise ValueError(f"invalid child range at NGTR node {node}")
        if (label == 0) != (node < 2):
            raise ValueError(f"invalid label at NGTR node {node}")
        if flags & 1:
            terminal, offset = read_var_uint32(data, offset)
            if node < 2 or terminal == 0:
                raise ValueError(f"invalid terminal count at NGTR node {node}")
            masses[node] = terminal
        elif node >= 2 and degree == 0:
            raise ValueError(f"missing terminal count at NGTR leaf {node}")
        child_starts[node] = next_child
        child_counts[node] = degree
        labels[node] = label
        next_child += degree
    if next_child != node_count or offset != len(data):
        raise ValueError("NGTR file length does not match its records")

    for node in range(node_count - 1, -1, -1):
        if not child_counts[node]:
            continue
        start = child_starts[node]
        stop = start + child_counts[node]
        mass = masses[node]
        previous_label = 0
        for child in range(start, stop):
            if labels[child] <= previous_label:
                raise ValueError(f"invalid child order at NGTR node {node}")
            previous_label = labels[child]
            mass += masses[child]
        if mass > UINT32_MAX:
            raise ValueError(f"mass overflows at NGTR node {node}")
        masses[node] = mass

    mass_offset = Struct("<I")
    for node, degree in enumerate(child_counts):
        if node % NODES_PER_CHECKPOINT == 0:
            cp = HEADER.size + node // NODES_PER_CHECKPOINT * CHECKPOINT.size
            mass_offset.pack_into(output, cp + 8, len(output) - mass_start)
        if degree:
            append_var_uint32(output, masses[node])
    return output


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trie", type=Path, help="input compact NGTR v3 model")
    parser.add_argument("index", type=Path, help="output NGIX index")
    args = parser.parse_args()
    try:
        if args.trie.resolve() == args.index.resolve() or (
            args.index.exists() and args.trie.samefile(args.index)
        ):
            raise ValueError("model and index must use different files")
        index = build_index(args.trie.read_bytes())
        args.index.write_bytes(index)
    except (OSError, ValueError) as error:
        parser.exit(1, f"{parser.prog}: {error}\n")


if __name__ == "__main__":
    main()
