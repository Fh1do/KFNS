#!/usr/bin/env python3
"""Generate the MasterVolume IPS from extracted firmware in this folder."""

import ctypes
import hashlib
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
INPUT_FOLDER = ROOT / "work"
TEMP_WORK = ROOT / ".mvp_work"
OUTPUT = ROOT / "patches"
AUDIO_TITLE_ID = "0100000000000014"


class NsoHeader(ctypes.Structure):
    TextCompress = 1 << 0
    RodataCompress = 1 << 1
    DataCompress = 1 << 2
    TextHash = 1 << 3
    RodataHash = 1 << 4
    DataHash = 1 << 5

    class SegmentHeader(ctypes.Structure):
        _fields_ = [("file_off", ctypes.c_uint32), ("memory_off", ctypes.c_uint32), ("size", ctypes.c_uint32)]

    class SegmentHeaderRelative(ctypes.Structure):
        _fields_ = [("offset", ctypes.c_uint32), ("size", ctypes.c_uint32)]

    _fields_ = [
        ("magic", ctypes.c_uint32), ("version", ctypes.c_uint32), ("reserved1", ctypes.c_uint32),
        ("flags", ctypes.c_uint32), ("text_hdr", SegmentHeader), ("module_name_off", ctypes.c_uint32),
        ("rodata_hdr", SegmentHeader), ("module_name_sz", ctypes.c_uint32), ("data_hdr", SegmentHeader),
        ("bss_sz", ctypes.c_uint32), ("build_id", ctypes.c_uint8 * 0x20),
        ("text_comp_sz", ctypes.c_uint32), ("rodata_comp_sz", ctypes.c_uint32),
        ("data_comp_sz", ctypes.c_uint32), ("reserved2", ctypes.c_uint8 * 0x1c),
        ("api_info_hdr", SegmentHeaderRelative), ("dynstr_info_hdr", SegmentHeaderRelative),
        ("dynsym_info_hdr", SegmentHeaderRelative), ("text_hash", ctypes.c_uint8 * 0x20),
        ("rodata_hash", ctypes.c_uint8 * 0x20), ("data_hash", ctypes.c_uint8 * 0x20),
    ]


assert ctypes.sizeof(NsoHeader) == 0x100


def find_prod_keys():
    if not INPUT_FOLDER.is_dir():
        return None
    matches = list(INPUT_FOLDER.rglob("prod.keys"))
    return matches[0] if len(matches) == 1 else None


def find_firmware_folder():
    """Find the extracted firmware under work, without using its version name."""
    if not INPUT_FOLDER.is_dir():
        return None
    if list(INPUT_FOLDER.glob("*.nca")):
        return INPUT_FOLDER

    ignored = {"out", ".mvp_work", "__pycache__", ".git"}
    candidates = []
    for folder in INPUT_FOLDER.iterdir():
        if not folder.is_dir() or folder.name.lower() in ignored:
            continue
        count = sum(1 for _ in folder.rglob("*.nca"))
        if count:
            candidates.append((count, folder))

    if not candidates:
        return None
    highest = max(count for count, _ in candidates)
    best = [folder for count, folder in candidates if count == highest]
    if len(best) != 1:
        raise ValueError("Hay más de una carpeta de firmware con archivos NCA.")
    return best[0]

def find_hactoolnet():
    for name in ("hactoolnet.exe", "hactoolnet_v0.2.0.exe", "hactoolnet"):
        candidate = ROOT / name
        if candidate.is_file():
            return candidate
    return None


def stage_ncas(firmware_folder, registered_folder):
    registered_folder.mkdir(parents=True, exist_ok=True)
    ncas = list(firmware_folder.rglob("*.nca"))
    if not ncas:
        return 0

    for nca in ncas:
        try:
            relative = nca.relative_to(firmware_folder)
        except ValueError:
            relative = Path(nca.name)
        destination = registered_folder / relative.name
        if destination.exists():
            continue
        try:
            os.link(str(nca), str(destination))
        except OSError:
            shutil.copy2(str(nca), str(destination))
    return len(ncas)


def extract_audio_nso(hactoolnet, keys, firmware_folder):
    switchfs = TEMP_WORK / "switchfs"
    registered = switchfs / "Contents" / "registered"
    count = stage_ncas(firmware_folder, registered)
    if not count:
        raise ValueError("No se encontraron archivos NCA en el firmware.")

    exefs = TEMP_WORK / "audio_exefs"
    exefs.mkdir(parents=True, exist_ok=True)
    command = [
        str(hactoolnet), "-k", str(keys), "-t", "switchfs", "--title", AUDIO_TITLE_ID,
        "--exefsdir", str(exefs), str(switchfs),
    ]
    result = subprocess.run(command, capture_output=True, text=True,
                            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    log = (result.stdout or "") + "\n" + (result.stderr or "")
    if result.returncode:
        lowered = log.lower()
        if any(message in lowered for message in (
            "unable to decrypt", "missing key for", "could not decrypt",
            "key area key not found", "missing key area key",
        )):
            raise RuntimeError("Prod.keys desactualizado.")
        raise ValueError("Firmware inválido.")

    for candidate in exefs.rglob("*"):
        if candidate.is_file() and read_nso_module_name(candidate) == "audio":
            return candidate
    raise ValueError("Firmware inválido.")


def read_nso_module_name(path):
    try:
        with path.open("rb") as stream:
            header_bytes = stream.read(0x100)
            if len(header_bytes) < 0x100 or header_bytes[:4] != b"NSO0":
                return None
            header = NsoHeader.from_buffer_copy(header_bytes)
            if not 0 < header.module_name_sz <= 0x1000:
                return None
            stream.seek(header.module_name_off)
            return stream.read(header.module_name_sz).decode("ascii", "ignore").strip("\0")
    except (OSError, UnicodeError, ValueError):
        return None


def find_clamp(text, capstone):
    cs = capstone
    decoder = cs.Cs(cs.CS_ARCH_ARM64, cs.CS_MODE_ARM | cs.CS_MODE_LITTLE_ENDIAN)
    decoder.detail = True
    offsets = []
    state = 0
    value_reg = None

    for offset in range(0, len(text), 4):
        try:
            instruction = next(decoder.disasm(text[offset:offset + 4], 0x7100000000))
        except StopIteration:
            continue

        if set(instruction.groups) & {
            cs.arm64.ARM64_GRP_JUMP, cs.arm64.ARM64_GRP_CALL, cs.arm64.ARM64_GRP_RET
        }:
            state = 0

        operands = instruction.operands
        if len(operands) not in (2, 3):
            continue
        reg0, reg1 = operands[0], operands[1]
        reg2 = operands[2] if len(operands) == 3 else None

        if state == 0:
            if (instruction.id == cs.arm64.ARM64_INS_ADD and
                    reg0.type == cs.arm64.ARM64_OP_REG and
                    reg1.type == cs.arm64.ARM64_OP_REG and
                    reg1.value.reg == cs.arm64.ARM64_REG_SP and reg2 is not None and
                    reg2.type == cs.arm64.ARM64_OP_IMM and reg2.value.imm == 8):
                state = 1
        elif state == 1:
            if instruction.id == cs.arm64.ARM64_INS_FMOV:
                if (reg0.type == cs.arm64.ARM64_OP_REG and
                        reg1.type == cs.arm64.ARM64_OP_FP and reg1.value.fp == 1.0):
                    state = 2
                    value_reg = reg0.value.reg
                else:
                    state = 0
        elif state == 2:
            if instruction.id == cs.arm64.ARM64_INS_FCMP:
                if (reg0.type == cs.arm64.ARM64_OP_REG and
                        reg0.value.reg in (cs.arm64.ARM64_REG_S0, value_reg) and
                        reg1.type == cs.arm64.ARM64_OP_REG and
                        reg1.value.reg in (cs.arm64.ARM64_REG_S0, value_reg)):
                    state = 3
                else:
                    state = 0
        elif state == 3:
            if (instruction.id == cs.arm64.ARM64_INS_FCSEL and reg2 is not None and
                    reg0.type == cs.arm64.ARM64_OP_REG and
                    reg1.type == cs.arm64.ARM64_OP_REG and reg1.value.reg == value_reg and
                    reg2.type == cs.arm64.ARM64_OP_REG and
                    reg2.value.reg == cs.arm64.ARM64_REG_S0 and
                    instruction.cc == cs.arm64.ARM64_CC_GT):
                offsets.append((offset, decoder.reg_name(reg0.value.reg)))
                state = 0

    return offsets[0] if len(offsets) == 1 else None


def create_ips(nso_path, output_folder):
    try:
        from lz4.block import decompress
        import capstone
        import keystone
    except ImportError as exc:
        raise RuntimeError(
            "Falta una dependencia de Python. Instala lz4, capstone y keystone-engine."
        ) from exc

    with nso_path.open("rb") as stream:
        header_bytes = stream.read(0x100)
        if len(header_bytes) != 0x100 or header_bytes[:4] != b"NSO0":
            raise ValueError("Firmware inválido.")
        header = NsoHeader.from_buffer_copy(header_bytes)
        build_id = bytes(header.build_id).rstrip(b"\0").hex()
        if header.module_name_sz:
            stream.seek(header.module_name_off)
            name = stream.read(header.module_name_sz).decode("ascii", "ignore").strip("\0")
            if name != "audio":
                raise ValueError("Firmware inválido.")
        stream.seek(header.text_hdr.file_off)
        text = stream.read(header.text_comp_sz)

    if header.flags & NsoHeader.TextCompress:
        try:
            text = decompress(text, uncompressed_size=header.text_hdr.size)
        except Exception as exc:
            raise ValueError("Firmware inválido.") from exc
    if len(text) != header.text_hdr.size:
        raise ValueError("Firmware inválido.")
    if header.flags & NsoHeader.TextHash:
        if hashlib.sha256(text).digest() != bytes(header.text_hash):
            raise ValueError("Firmware inválido.")

    match = find_clamp(text, capstone)
    if match is None:
        raise ValueError("No se encontró de forma única la instrucción MasterVolume para parchear.")
    offset, register = match

    assembler = keystone.Ks(keystone.KS_ARCH_ARM64, keystone.KS_MODE_LITTLE_ENDIAN)
    instructions, _ = assembler.asm("fmov {}, s0".format(register))
    if len(instructions) > 0xFFFF or offset + ctypes.sizeof(NsoHeader) > 0xFFFFFF:
        raise ValueError("El offset del parche no cabe en el formato IPS.")

    patch = (b"PATCH" + (offset + ctypes.sizeof(NsoHeader)).to_bytes(3, "big") +
             len(instructions).to_bytes(2, "big") + bytes(instructions) + b"EOF")
    output_folder.mkdir(parents=True, exist_ok=True)
    output_path = output_folder / (build_id + ".ips")
    output_path.write_bytes(patch)
    return output_path


def main():
    try:
        keys = find_prod_keys()
        firmware = find_firmware_folder()
        hactoolnet = find_hactoolnet()
        OUTPUT.mkdir(parents=True, exist_ok=True)
        if keys is None or not keys.is_file() or firmware is None:
            print("Firmware or prod.keys are missing from the MasterVolumePatcher work folder.")
            return 0
        if hactoolnet is None:
            print("No se encontró hactoolnet en la raíz del programa.")
            return 2

        TEMP_WORK.mkdir(parents=True, exist_ok=True)
        nso = extract_audio_nso(hactoolnet, keys, firmware)
        patch = create_ips(nso, OUTPUT)
        print("Generated patch: {}".format(patch.name))
        return 0
    except RuntimeError as exc:
        print(str(exc))
        return 3
    except ValueError as exc:
        print(str(exc))
        return 4
    except OSError as exc:
        print("Error de archivos: {}".format(exc))
        return 5
    finally:
        shutil.rmtree(TEMP_WORK, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())


