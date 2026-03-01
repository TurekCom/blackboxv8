# -*- coding: UTF-8 -*-
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, List, Literal, Optional, Sequence


SID_START = 0xD400
SID_END = 0xD418


@dataclass
class SidWriteSite:
    bank: int
    pc: int
    opcode: int
    addr: int
    addressing: str


@dataclass
class ProbeResult:
    bank: int
    entry: str
    entry_pc: int
    steps: int
    sid_writes: List[tuple[int, int]]
    bank_switches: List[tuple[int, int]]
    c000_writes: List[tuple[int, int]]


@dataclass
class BootstrapStubResult:
    entry_pc: int
    final_pc: int
    active_bank: int
    sid_writes_delta: List[tuple[int, int]]


@dataclass
class BootstrapAnalysis:
    cold_probe: ProbeResult
    ca_bytes: bytes
    stubs: List[BootstrapStubResult]


@dataclass
class CommandFlowResult:
    command_key: int
    final_pc: int
    active_bank: int
    sid_writes_delta: List[tuple[int, int]]
    bank_switches_delta: List[tuple[int, int]]
    chout_text: str
    ram_exec_hits: List[int]


@dataclass
class CommandSequenceResult:
    steps: List[CommandFlowResult]
    final_pc: int
    active_bank: int


@dataclass
class CommandScanItem:
    command_key: int
    final_pc: int
    active_bank: int
    sid_delta_count: int
    sid_nonzero_count: int
    bank_switch_delta_count: int
    chout_len: int
    chout_tail: str
    ram_exec_hits_count: int
    ram_exec_last: int


class TraceMemory(list):
    def __init__(self, init: Iterable[int]):
        super().__init__(init)
        self.sid_writes: List[tuple[int, int]] = []

    def __setitem__(self, key, value):
        if isinstance(key, int) and SID_START <= key <= SID_END:
            self.sid_writes.append((key, value & 0xFF))
        super().__setitem__(key, value)


class BankedC64Memory:
    def __init__(
        self,
        banks: List[bytes],
        initial_bank: int,
        fill: int = 0x60,
    ):
        self.ram = bytearray([fill] * 65536)
        self.banks = banks
        self.active_bank = initial_bank % len(self.banks)
        self.sid_writes: List[tuple[int, int]] = []
        self.bank_switches: List[tuple[int, int]] = []
        self.c000_writes: List[tuple[int, int]] = []

    def __len__(self):
        return 65536

    def _is_rom_window(self, addr: int) -> bool:
        return 0x8000 <= addr <= 0xBFFF

    def __getitem__(self, key):
        if isinstance(key, slice):
            start, stop, step = key.indices(65536)
            return [self[i] for i in range(start, stop, step)]
        addr = key & 0xFFFF
        if self._is_rom_window(addr):
            return self.banks[self.active_bank][addr - 0x8000]
        return self.ram[addr]

    def __setitem__(self, key, value):
        if isinstance(key, slice):
            start, stop, step = key.indices(65536)
            if step != 1:
                indexes = list(range(start, stop, step))
                if len(indexes) != len(value):
                    raise ValueError("attempt to assign sequence of size mismatch")
                for i, v in zip(indexes, value):
                    self[i] = v
                return
            data = bytes(value)
            for i, v in enumerate(data):
                self[start + i] = v
            return

        addr = key & 0xFFFF
        v = value & 0xFF
        if SID_START <= addr <= SID_END:
            self.sid_writes.append((addr, v))
        if addr in (0xDFFF, 0xDE00):
            self.active_bank = v & (len(self.banks) - 1)
            self.bank_switches.append((addr, v))
        if 0xC000 <= addr <= 0xCFFF:
            self.c000_writes.append((addr, v))
        self.ram[addr] = v

    def clone(self) -> "BankedC64Memory":
        other = BankedC64Memory(self.banks, self.active_bank, fill=0x60)
        other.ram = bytearray(self.ram)
        other.sid_writes = list(self.sid_writes)
        other.bank_switches = list(self.bank_switches)
        other.c000_writes = list(self.c000_writes)
        return other


class BlackboxROM:
    def __init__(self, rom_path: Path | str):
        self.rom_path = Path(rom_path)
        self.raw = self.rom_path.read_bytes()
        if len(self.raw) not in (32768, 65536):
            raise ValueError(f"Unsupported ROM size: {len(self.raw)} bytes")

    @property
    def is_64k(self) -> bool:
        return len(self.raw) == 65536

    @property
    def banks(self) -> List[bytes]:
        if self.is_64k:
            return [self.raw[i * 0x4000 : (i + 1) * 0x4000] for i in range(4)]
        return [self.raw[:0x4000], self.raw[0x4000:0x8000]]

    def bank(self, bank_idx: int) -> bytes:
        b = self.banks
        if bank_idx < 0 or bank_idx >= len(b):
            raise IndexError(f"Bank {bank_idx} out of range (0..{len(b)-1})")
        return b[bank_idx]

    def entry_points(self, bank_idx: int) -> tuple[int, int]:
        b = self.bank(bank_idx)
        cold = b[0] | (b[1] << 8)
        warm = b[2] | (b[3] << 8)
        return cold, warm

    def find_sid_write_sites(self, bank_idx: Optional[int] = None) -> List[SidWriteSite]:
        banks = [bank_idx] if bank_idx is not None else list(range(len(self.banks)))
        sites: List[SidWriteSite] = []
        for idx in banks:
            data = self.bank(idx)
            for off in range(len(data) - 2):
                op = data[off]
                # STA abs, STX abs, STY abs
                if op in (0x8D, 0x8E, 0x8C):
                    addr = data[off + 1] | (data[off + 2] << 8)
                    if SID_START <= addr <= SID_END:
                        sites.append(
                            SidWriteSite(
                                bank=idx,
                                pc=0x8000 + off,
                                opcode=op,
                                addr=addr,
                                addressing="abs",
                            )
                        )
                # STA abs,X and STA abs,Y
                elif op in (0x9D, 0x99):
                    base = data[off + 1] | (data[off + 2] << 8)
                    # Potential SID hit with index, not exact.
                    if (SID_START - 0xFF) <= base <= SID_END:
                        sites.append(
                            SidWriteSite(
                                bank=idx,
                                pc=0x8000 + off,
                                opcode=op,
                                addr=base,
                                addressing="abs,indexed",
                            )
                        )
        return sites

    def find_xrefs_to(self, target_addr: int, bank_idx: Optional[int] = None) -> List[tuple[int, int, str]]:
        banks = [bank_idx] if bank_idx is not None else list(range(len(self.banks)))
        out: List[tuple[int, int, str]] = []
        for idx in banks:
            data = self.bank(idx)
            for off in range(len(data) - 2):
                op = data[off]
                addr = data[off + 1] | (data[off + 2] << 8)
                if addr != target_addr:
                    continue
                if op == 0x20:
                    out.append((idx, 0x8000 + off, "JSR"))
                elif op == 0x4C:
                    out.append((idx, 0x8000 + off, "JMP"))
                elif op == 0x6C:
                    out.append((idx, 0x8000 + off, "JMP_IND"))
        return out

    def _apply_ram_profile(self, mem, profile: str | None) -> None:
        if not profile:
            return
        p = profile.strip().lower()
        # Copy routine used by 0x3F/0x01 path from bank1 into low RAM.
        if p in ("cmd_core", "speech_cmd_core"):
            bank1 = self.bank(1)
            # 0x8500.. mapped to 0x0334.. (observed in runtime trace)
            core = bank1[0x0500:0x05A6]
            for i, b in enumerate(core):
                mem[0x0334 + i] = b
            return
        # Hypothesis profile: include cmd core + potential continuation at $0900.
        if p in ("cmd_plus_0900", "speech_hyp_0900"):
            self._apply_ram_profile(mem, "cmd_core")
            bank1 = self.bank(1)
            block = bank1[0x1100:0x1400]  # ROM $9100-$93FF
            for i, b in enumerate(block):
                mem[0x0900 + i] = b
            return
        raise ValueError(f"Unknown RAM profile: {profile}")

    def probe_entry(
        self,
        bank_idx: int = 1,
        entry: Literal["cold", "warm"] = "warm",
        steps: int = 30000,
        patch_kernal: bool = True,
        cia_dc01: int = 0xEF,
        ram_fill: int = 0x60,
    ) -> ProbeResult:
        try:
            from py65.devices.mpu6502 import MPU
        except Exception as e:
            raise RuntimeError("py65 is required for probe_entry") from e

        bank = self.bank(bank_idx)
        cold, warm = self.entry_points(bank_idx)
        start_pc = cold if entry == "cold" else warm

        if self.is_64k:
            mem = BankedC64Memory(self.banks, initial_bank=bank_idx, fill=ram_fill)
        else:
            mem = TraceMemory([ram_fill & 0xFF] * 65536)
            mem[0x8000:0xC000] = bank
        mem[0x0000] = 0x2F
        mem[0x0001] = 0x37
        mem[0xDC01] = cia_dc01 & 0xFF

        if patch_kernal:
            # Najczęściej wywoływane rutyny KERNAL/BASIC podczas cold/warm start
            # cartridge, stubowane jako RTS aby umożliwić dalszy przepływ.
            for addr in (0xFDA3, 0xFD15, 0xFF5B, 0xE453, 0xE3BF, 0xE422, 0xE39D):
                mem[addr] = 0x60

        mpu = MPU(memory=mem)
        mpu.pc = start_pc
        for _ in range(steps):
            try:
                mpu.step()
            except Exception:
                break

        sid_writes = list(mem.sid_writes) if hasattr(mem, "sid_writes") else []
        bank_switches = list(mem.bank_switches) if hasattr(mem, "bank_switches") else []
        c000_writes = list(mem.c000_writes) if hasattr(mem, "c000_writes") else []
        return ProbeResult(
            bank=bank_idx,
            entry=entry,
            entry_pc=start_pc,
            steps=steps,
            sid_writes=sid_writes,
            bank_switches=bank_switches,
            c000_writes=c000_writes,
        )

    def probe_pc(
        self,
        bank_idx: int,
        pc: int,
        steps: int = 6000,
        patch_kernal: bool = True,
        a: int = 0,
        x: int = 0,
        y: int = 0,
        cia_dc01: int = 0xEF,
        ram_fill: int = 0x60,
    ) -> ProbeResult:
        try:
            from py65.devices.mpu6502 import MPU
        except Exception as e:
            raise RuntimeError("py65 is required for probe_pc") from e

        bank = self.bank(bank_idx)
        if self.is_64k:
            mem = BankedC64Memory(self.banks, initial_bank=bank_idx, fill=ram_fill)
        else:
            mem = TraceMemory([ram_fill & 0xFF] * 65536)
            mem[0x8000:0xC000] = bank
        mem[0x0000] = 0x2F
        mem[0x0001] = 0x37
        mem[0xDC01] = cia_dc01 & 0xFF

        if patch_kernal:
            for addr in (0xFDA3, 0xFD15, 0xFF5B, 0xE453, 0xE3BF, 0xE422, 0xE39D):
                mem[addr] = 0x60

        # Fail-safe return target for RTS chains.
        mem[0x02FE] = 0x00
        mem[0x02FF] = 0x80

        mpu = MPU(memory=mem)
        mpu.pc = pc
        mpu.a = a & 0xFF
        mpu.x = x & 0xFF
        mpu.y = y & 0xFF
        for _ in range(steps):
            try:
                mpu.step()
            except Exception:
                break

        sid_writes = list(mem.sid_writes) if hasattr(mem, "sid_writes") else []
        bank_switches = list(mem.bank_switches) if hasattr(mem, "bank_switches") else []
        c000_writes = list(mem.c000_writes) if hasattr(mem, "c000_writes") else []
        return ProbeResult(
            bank=bank_idx,
            entry=f"pc:${pc:04X}",
            entry_pc=pc,
            steps=steps,
            sid_writes=sid_writes,
            bank_switches=bank_switches,
            c000_writes=c000_writes,
        )

    def analyze_bootstrap_stubs(
        self,
        bank_idx: int = 1,
        cold_steps: int = 140000,
        stub_steps: int = 3000,
        cia_dc01: int = 0xEF,
        stub_entries: Optional[List[int]] = None,
        ram_fill: int = 0x60,
    ) -> BootstrapAnalysis:
        try:
            from py65.devices.mpu6502 import MPU
        except Exception as e:
            raise RuntimeError("py65 is required for analyze_bootstrap_stubs") from e

        if not self.is_64k:
            raise RuntimeError("analyze_bootstrap_stubs currently expects 64KB ROM.")

        mem = BankedC64Memory(self.banks, initial_bank=bank_idx, fill=ram_fill)
        mem[0x0000] = 0x2F
        mem[0x0001] = 0x37
        mem[0xDC01] = cia_dc01 & 0xFF
        for addr in (0xFDA3, 0xFD15, 0xFF5B, 0xE453, 0xE3BF, 0xE422, 0xE39D, 0xFCE7, 0xFCEF, 0xFFD2):
            mem[addr] = 0x60

        mpu = MPU(memory=mem)
        cold_pc = self.entry_points(bank_idx)[0]
        mpu.pc = cold_pc
        for _ in range(cold_steps):
            try:
                mpu.step()
            except Exception:
                break

        cold_probe = ProbeResult(
            bank=bank_idx,
            entry="cold",
            entry_pc=cold_pc,
            steps=cold_steps,
            sid_writes=list(mem.sid_writes),
            bank_switches=list(mem.bank_switches),
            c000_writes=list(mem.c000_writes),
        )

        ca = bytes(mem[0xCA00 + i] for i in range(0x20))
        entries = stub_entries or [0xCA00, 0xCA07, 0xCA0E, 0xCA15]
        stubs: List[BootstrapStubResult] = []
        sid_cursor = len(mem.sid_writes)
        for entry in entries:
            mpu.pc = entry
            for _ in range(stub_steps):
                try:
                    mpu.step()
                except Exception:
                    break
            sid_delta = list(mem.sid_writes[sid_cursor:])
            sid_cursor = len(mem.sid_writes)
            stubs.append(
                BootstrapStubResult(
                    entry_pc=entry,
                    final_pc=mpu.pc,
                    active_bank=mem.active_bank,
                    sid_writes_delta=sid_delta,
                )
            )

        return BootstrapAnalysis(cold_probe=cold_probe, ca_bytes=ca, stubs=stubs)

    @staticmethod
    def _rts_from_jsr(mpu, mem) -> None:
        sp = mpu.sp
        lo = mem[0x0100 + ((sp + 1) & 0xFF)]
        hi = mem[0x0100 + ((sp + 2) & 0xFF)]
        mpu.sp = (sp + 2) & 0xFF
        mpu.pc = (((hi << 8) | lo) + 1) & 0xFFFF

    @staticmethod
    def _set_zero_flag(mpu, is_zero: bool) -> None:
        if is_zero:
            mpu.p |= mpu.ZERO
        else:
            mpu.p &= (~mpu.ZERO) & 0xFF

    @staticmethod
    def _pop_keyboard_buffer(mem) -> int:
        count = mem[0x00C6] & 0xFF
        if count == 0:
            return 0
        val = mem[0x0277] & 0xFF
        for i in range(count - 1):
            mem[0x0277 + i] = mem[0x0277 + i + 1]
        mem[0x0277 + count - 1] = 0
        mem[0x00C6] = (count - 1) & 0xFF
        return val

    def _call_subroutine(self, mpu, mem, target: int, max_steps: int = 200) -> None:
        if target < 0x0200:
            return
        ret = (mpu.pc - 1) & 0xFFFF
        mem[0x0100 + mpu.sp] = (ret >> 8) & 0xFF
        mpu.sp = (mpu.sp - 1) & 0xFF
        mem[0x0100 + mpu.sp] = ret & 0xFF
        mpu.sp = (mpu.sp - 1) & 0xFF
        start_sp = mpu.sp
        mpu.pc = target & 0xFFFF
        self._step_with_kernal_hooks(
            mpu,
            mem,
            max_steps,
            input_bytes=[],
            chout=[],
            irq_period=0,
            ram_exec_hits=None,
        )
        # Best effort: if subroutine didn't unwind correctly, avoid deep stack drift.
        if ((mpu.sp - start_sp) & 0xFF) > 0x40:
            mpu.sp = start_sp

    def _step_with_kernal_hooks(
        self,
        mpu,
        mem,
        steps: int,
        input_bytes: Optional[List[int]] = None,
        chout: Optional[List[int]] = None,
        irq_period: int = 0,
        ram_exec_hits: Optional[List[int]] = None,
        emulate_keyscan: bool = True,
    ) -> None:
        inq = input_bytes if input_bytes is not None else []
        out = chout if chout is not None else []
        irq_counter = 0
        for _ in range(steps):
            if emulate_keyscan and (mem[0x00CB] & 0xFF) == 0x40 and (mem[0x00C6] & 0xFF) > 0:
                mem[0x00CB] = self._pop_keyboard_buffer(mem)

            if irq_period > 0:
                irq_counter += 1
                if irq_counter >= irq_period:
                    irq_counter = 0
                    irq_vec = (mem[0x0314] & 0xFF) | ((mem[0x0315] & 0xFF) << 8)
                    self._call_subroutine(mpu, mem, irq_vec, max_steps=120)

            pc = mpu.pc
            if ram_exec_hits is not None and 0x0000 <= pc < 0x0800:
                if not ram_exec_hits or ram_exec_hits[-1] != pc:
                    ram_exec_hits.append(pc)
            if pc == 0xFFD2:
                out.append(mpu.a & 0xFF)
                self._rts_from_jsr(mpu, mem)
                continue
            if pc in (0xFFCF, 0xFFE4):
                a = inq.pop(0) if inq else self._pop_keyboard_buffer(mem)
                mpu.a = a
                self._set_zero_flag(mpu, a == 0)
                self._rts_from_jsr(mpu, mem)
                continue
            if pc == 0xFFE1:
                # STOP: return non-zero by default (continue running).
                mpu.a = 1
                self._set_zero_flag(mpu, False)
                self._rts_from_jsr(mpu, mem)
                continue
            try:
                mpu.step()
            except Exception:
                break

    @staticmethod
    def _cpu_copy(dst, src) -> None:
        dst.a = src.a
        dst.x = src.x
        dst.y = src.y
        dst.p = src.p
        dst.sp = src.sp

    def probe_command_flow(
        self,
        command_key: int,
        command_input: Sequence[int] | bytes | str = b"",
        keyboard_buffer: Sequence[int] | bytes | str = b"",
        irq_period: int = 0,
        dispatch_pc: int = 0xAF70,
        use_keyscan: bool = False,
        ram_profile: Optional[str] = None,
        cold_bank_idx: int = 1,
        cold_steps: int = 140000,
        command_steps: int = 40000,
        cia_dc01: int = 0xEF,
        ram_fill: int = 0x60,
    ) -> CommandFlowResult:
        try:
            from py65.devices.mpu6502 import MPU
        except Exception as e:
            raise RuntimeError("py65 is required for probe_command_flow") from e

        if not self.is_64k:
            raise RuntimeError("probe_command_flow currently expects 64KB ROM.")

        if isinstance(command_input, str):
            inq = list(command_input.encode("latin-1", errors="ignore"))
        else:
            inq = list(command_input)
        if isinstance(keyboard_buffer, str):
            kbuf = list(keyboard_buffer.encode("latin-1", errors="ignore"))
        else:
            kbuf = list(keyboard_buffer)

        mem = BankedC64Memory(self.banks, initial_bank=cold_bank_idx, fill=ram_fill)
        mem[0x0000] = 0x2F
        mem[0x0001] = 0x37
        mem[0xDC01] = cia_dc01 & 0xFF
        mpu = MPU(memory=mem)

        # 1) Cold start ROM.
        mpu.pc = self.entry_points(cold_bank_idx)[0]
        chout: List[int] = []
        ram_exec_hits: List[int] = []
        self._step_with_kernal_hooks(
            mpu,
            mem,
            cold_steps,
            input_bytes=[],
            chout=chout,
            irq_period=irq_period,
            ram_exec_hits=ram_exec_hits,
            emulate_keyscan=True,
        )
        self._apply_ram_profile(mem, ram_profile)

        # 2) Trigger dispatcher with selected key code.
        sid_cursor = len(mem.sid_writes)
        sw_cursor = len(mem.bank_switches)
        if use_keyscan:
            mem[0x00CB] = 0x40
            kbuf = [command_key & 0xFF] + kbuf
        else:
            mem[0x00CB] = command_key & 0xFF
        if kbuf:
            # C64 keyboard queue in RAM
            mem[0x00C6] = min(255, len(kbuf))
            for i, b in enumerate(kbuf[:255]):
                mem[0x0277 + i] = b & 0xFF
        mpu.pc = dispatch_pc & 0xFFFF
        self._step_with_kernal_hooks(
            mpu,
            mem,
            command_steps,
            input_bytes=inq,
            chout=chout,
            irq_period=irq_period,
            ram_exec_hits=ram_exec_hits,
            emulate_keyscan=True,
        )

        sid_delta = list(mem.sid_writes[sid_cursor:])
        sw_delta = list(mem.bank_switches[sw_cursor:])
        text = bytes(c for c in chout if 32 <= c <= 126 or c in (10, 13)).decode(
            "latin-1", errors="ignore"
        )
        return CommandFlowResult(
            command_key=command_key & 0xFF,
            final_pc=mpu.pc,
            active_bank=mem.active_bank,
            sid_writes_delta=sid_delta,
            bank_switches_delta=sw_delta,
            chout_text=text,
            ram_exec_hits=ram_exec_hits,
        )

    def probe_command_sequence(
        self,
        command_keys: Sequence[int],
        command_inputs: Optional[Sequence[Sequence[int] | bytes | str]] = None,
        keyboard_buffers: Optional[Sequence[Sequence[int] | bytes | str]] = None,
        irq_period: int = 0,
        dispatch_pc: int = 0xAF70,
        use_keyscan: bool = False,
        ram_profile: Optional[str] = None,
        cold_bank_idx: int = 1,
        cold_steps: int = 140000,
        command_steps: int = 40000,
        cia_dc01: int = 0xEF,
        ram_fill: int = 0x60,
    ) -> CommandSequenceResult:
        try:
            from py65.devices.mpu6502 import MPU
        except Exception as e:
            raise RuntimeError("py65 is required for probe_command_sequence") from e

        if not self.is_64k:
            raise RuntimeError("probe_command_sequence currently expects 64KB ROM.")

        inputs = list(command_inputs) if command_inputs is not None else [b""] * len(command_keys)
        if len(inputs) < len(command_keys):
            inputs += [b""] * (len(command_keys) - len(inputs))
        kbds = list(keyboard_buffers) if keyboard_buffers is not None else [b""] * len(command_keys)
        if len(kbds) < len(command_keys):
            kbds += [b""] * (len(command_keys) - len(kbds))

        def _to_bytes(seq: Sequence[int] | bytes | str) -> List[int]:
            if isinstance(seq, str):
                return list(seq.encode("latin-1", errors="ignore"))
            return list(seq)

        mem = BankedC64Memory(self.banks, initial_bank=cold_bank_idx, fill=ram_fill)
        mem[0x0000] = 0x2F
        mem[0x0001] = 0x37
        mem[0xDC01] = cia_dc01 & 0xFF
        mpu = MPU(memory=mem)

        # Cold start
        mpu.pc = self.entry_points(cold_bank_idx)[0]
        chout: List[int] = []
        ram_exec_hits: List[int] = []
        self._step_with_kernal_hooks(
            mpu,
            mem,
            cold_steps,
            input_bytes=[],
            chout=chout,
            irq_period=irq_period,
            ram_exec_hits=ram_exec_hits,
            emulate_keyscan=True,
        )
        self._apply_ram_profile(mem, ram_profile)

        step_results: List[CommandFlowResult] = []
        ch_cursor = 0
        for idx, key in enumerate(command_keys):
            sid_cursor = len(mem.sid_writes)
            sw_cursor = len(mem.bank_switches)
            kbuf = _to_bytes(kbds[idx])
            if use_keyscan:
                mem[0x00CB] = 0x40
                kbuf = [key & 0xFF] + kbuf
            else:
                mem[0x00CB] = key & 0xFF
            if kbuf:
                mem[0x00C6] = min(255, len(kbuf))
                for i, b in enumerate(kbuf[:255]):
                    mem[0x0277 + i] = b & 0xFF
            mpu.pc = dispatch_pc & 0xFFFF
            self._step_with_kernal_hooks(
                mpu,
                mem,
                command_steps,
                input_bytes=_to_bytes(inputs[idx]),
                chout=chout,
                irq_period=irq_period,
                ram_exec_hits=ram_exec_hits,
                emulate_keyscan=True,
            )
            sid_delta = list(mem.sid_writes[sid_cursor:])
            sw_delta = list(mem.bank_switches[sw_cursor:])
            ch_delta = chout[ch_cursor:]
            ch_cursor = len(chout)
            text = bytes(c for c in ch_delta if 32 <= c <= 126 or c in (10, 13)).decode(
                "latin-1", errors="ignore"
            )
            step_results.append(
                CommandFlowResult(
                    command_key=key & 0xFF,
                    final_pc=mpu.pc,
                    active_bank=mem.active_bank,
                    sid_writes_delta=sid_delta,
                    bank_switches_delta=sw_delta,
                    chout_text=text,
                    ram_exec_hits=list(ram_exec_hits),
                )
            )

        return CommandSequenceResult(steps=step_results, final_pc=mpu.pc, active_bank=mem.active_bank)

    def scan_command_keys(
        self,
        keys: Sequence[int],
        irq_period: int = 0,
        dispatch_pc: int = 0xAF70,
        use_keyscan: bool = False,
        ram_profile: Optional[str] = None,
        cold_bank_idx: int = 1,
        cold_steps: int = 140000,
        command_steps: int = 20000,
        cia_dc01: int = 0xEF,
        ram_fill: int = 0x60,
    ) -> List[CommandScanItem]:
        try:
            from py65.devices.mpu6502 import MPU
        except Exception as e:
            raise RuntimeError("py65 is required for scan_command_keys") from e

        if not self.is_64k:
            raise RuntimeError("scan_command_keys currently expects 64KB ROM.")

        base_mem = BankedC64Memory(self.banks, initial_bank=cold_bank_idx, fill=ram_fill)
        base_mem[0x0000] = 0x2F
        base_mem[0x0001] = 0x37
        base_mem[0xDC01] = cia_dc01 & 0xFF
        base_cpu = MPU(memory=base_mem)

        # Cold boot baseline
        base_cpu.pc = self.entry_points(cold_bank_idx)[0]
        cold_chout: List[int] = []
        self._step_with_kernal_hooks(
            base_cpu,
            base_mem,
            cold_steps,
            input_bytes=[],
            chout=cold_chout,
            irq_period=irq_period,
            emulate_keyscan=True,
        )
        self._apply_ram_profile(base_mem, ram_profile)

        baseline_sid = len(base_mem.sid_writes)
        baseline_sw = len(base_mem.bank_switches)
        out: List[CommandScanItem] = []

        for key in keys:
            mem = base_mem.clone()
            cpu = MPU(memory=mem)
            self._cpu_copy(cpu, base_cpu)
            cpu.pc = dispatch_pc & 0xFFFF
            if use_keyscan:
                mem[0x00CB] = 0x40
                mem[0x00C6] = 1
                mem[0x0277] = key & 0xFF
            else:
                mem[0x00CB] = key & 0xFF

            chout: List[int] = []
            ram_exec_hits: List[int] = []
            self._step_with_kernal_hooks(
                cpu,
                mem,
                command_steps,
                input_bytes=[],
                chout=chout,
                irq_period=irq_period,
                ram_exec_hits=ram_exec_hits,
                emulate_keyscan=True,
            )

            sid_delta = mem.sid_writes[baseline_sid:]
            sw_delta = mem.bank_switches[baseline_sw:]
            sid_nonzero = sum(1 for _, v in sid_delta if v != 0)
            text = bytes(c for c in chout if 32 <= c <= 126 or c in (10, 13)).decode(
                "latin-1", errors="ignore"
            )
            out.append(
                CommandScanItem(
                    command_key=key & 0xFF,
                    final_pc=cpu.pc,
                    active_bank=mem.active_bank,
                    sid_delta_count=len(sid_delta),
                    sid_nonzero_count=sid_nonzero,
                    bank_switch_delta_count=len(sw_delta),
                    chout_len=len(chout),
                    chout_tail=text[-160:],
                    ram_exec_hits_count=len(ram_exec_hits),
                    ram_exec_last=(ram_exec_hits[-1] if ram_exec_hits else -1),
                )
            )

        return out

    def scan_followup_keys(
        self,
        initial_key: int,
        keys: Sequence[int],
        initial_input: Sequence[int] | bytes | str = b"",
        initial_keyboard_buffer: Sequence[int] | bytes | str = b"",
        irq_period: int = 0,
        dispatch_pc: int = 0xAF70,
        use_keyscan: bool = False,
        ram_profile: Optional[str] = None,
        cold_bank_idx: int = 1,
        cold_steps: int = 140000,
        initial_steps: int = 40000,
        command_steps: int = 20000,
        cia_dc01: int = 0xEF,
        ram_fill: int = 0x60,
    ) -> List[CommandScanItem]:
        try:
            from py65.devices.mpu6502 import MPU
        except Exception as e:
            raise RuntimeError("py65 is required for scan_followup_keys") from e

        if not self.is_64k:
            raise RuntimeError("scan_followup_keys currently expects 64KB ROM.")

        def _to_bytes(seq: Sequence[int] | bytes | str) -> List[int]:
            if isinstance(seq, str):
                return list(seq.encode("latin-1", errors="ignore"))
            return list(seq)

        # 1) Build post-cold baseline.
        base_mem = BankedC64Memory(self.banks, initial_bank=cold_bank_idx, fill=ram_fill)
        base_mem[0x0000] = 0x2F
        base_mem[0x0001] = 0x37
        base_mem[0xDC01] = cia_dc01 & 0xFF
        base_cpu = MPU(memory=base_mem)
        base_cpu.pc = self.entry_points(cold_bank_idx)[0]
        cold_chout: List[int] = []
        self._step_with_kernal_hooks(
            base_cpu,
            base_mem,
            cold_steps,
            input_bytes=[],
            chout=cold_chout,
            irq_period=irq_period,
            emulate_keyscan=True,
        )
        self._apply_ram_profile(base_mem, ram_profile)

        # 2) Enter follow-up mode by executing initial key once.
        inq = _to_bytes(initial_input)
        kbuf = _to_bytes(initial_keyboard_buffer)
        if use_keyscan:
            base_mem[0x00CB] = 0x40
            kbuf = [initial_key & 0xFF] + kbuf
        else:
            base_mem[0x00CB] = initial_key & 0xFF
        if kbuf:
            base_mem[0x00C6] = min(255, len(kbuf))
            for i, b in enumerate(kbuf[:255]):
                base_mem[0x0277 + i] = b & 0xFF
        base_cpu.pc = dispatch_pc & 0xFFFF
        pre_chout: List[int] = []
        pre_hits: List[int] = []
        self._step_with_kernal_hooks(
            base_cpu,
            base_mem,
            initial_steps,
            input_bytes=inq,
            chout=pre_chout,
            irq_period=irq_period,
            ram_exec_hits=pre_hits,
            emulate_keyscan=True,
        )

        # 3) Scan follow-up keys from cloned post-initial state.
        baseline_sid = len(base_mem.sid_writes)
        baseline_sw = len(base_mem.bank_switches)
        out: List[CommandScanItem] = []
        for key in keys:
            mem = base_mem.clone()
            cpu = MPU(memory=mem)
            self._cpu_copy(cpu, base_cpu)
            cpu.pc = dispatch_pc & 0xFFFF
            if use_keyscan:
                mem[0x00CB] = 0x40
                mem[0x00C6] = 1
                mem[0x0277] = key & 0xFF
            else:
                mem[0x00CB] = key & 0xFF

            chout: List[int] = []
            ram_exec_hits: List[int] = []
            self._step_with_kernal_hooks(
                cpu,
                mem,
                command_steps,
                input_bytes=[],
                chout=chout,
                irq_period=irq_period,
                ram_exec_hits=ram_exec_hits,
                emulate_keyscan=True,
            )

            sid_delta = mem.sid_writes[baseline_sid:]
            sw_delta = mem.bank_switches[baseline_sw:]
            sid_nonzero = sum(1 for _, v in sid_delta if v != 0)
            text = bytes(c for c in chout if 32 <= c <= 126 or c in (10, 13)).decode(
                "latin-1", errors="ignore"
            )
            out.append(
                CommandScanItem(
                    command_key=key & 0xFF,
                    final_pc=cpu.pc,
                    active_bank=mem.active_bank,
                    sid_delta_count=len(sid_delta),
                    sid_nonzero_count=sid_nonzero,
                    bank_switch_delta_count=len(sw_delta),
                    chout_len=len(chout),
                    chout_tail=text[-160:],
                    ram_exec_hits_count=len(ram_exec_hits),
                    ram_exec_last=(ram_exec_hits[-1] if ram_exec_hits else -1),
                )
            )

        return out
