// Korrigiert relative Near-CALL/JMP (E8/E9), deren Ziel Ghidra ohne 64K-Wraparound
// berechnet hat. Richtiges Ziel: CS:((IP_next + rel16) & 0xFFFF).
// Setzt eine Override-Referenz (wirkt im Decompiler), legt fehlende Funktionen an
// und startet die Analyse für die Änderungen neu.
//@category MadTV

import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.*;

public class FixNearCalls extends GhidraScript {
    @Override
    public void run() throws Exception {
        ReferenceManager rm = currentProgram.getReferenceManager();
        int fixed = 0, created = 0;

        for (Instruction ins : currentProgram.getListing().getInstructions(true)) {
            byte op = ins.getByte(0);
            if (op == (byte) 0x9A) {
                // CALLF: Ziel stimmt, aber im per switch freigelegten Code fehlen oft Funktionen.
                for (Address t : ins.getFlows()) {
                    if (getFunctionAt(t) != null) continue;
                    if (getInstructionAt(t) == null) disassemble(t);
                    if (new CreateFunctionCmd(t).applyTo(currentProgram, monitor)) created++;
                    else println("WARN: Funktion bei " + t + " (CALLF " + ins.getAddress() + ") nicht anlegbar");
                }
                continue;
            }
            boolean isCall = op == (byte) 0xE8;
            boolean isJmp = op == (byte) 0xE9;
            if (!isCall && !isJmp) continue;

            SegmentedAddress at = (SegmentedAddress) ins.getAddress();
            // Das echte CS ist das Segment des Speicherblocks; Ghidra legt manche Instruktionen
            // mit normalisiertem Segment an (z.B. 3000:xxxx statt 3b0a:yyyy).
            int seg = (int) (currentProgram.getMemory().getBlock(at).getStart().getOffset() >> 4);
            int ip = (int) (at.getOffset() - ((long) seg << 4));
            int rel = (short) ((ins.getByte(1) & 0xff) | (ins.getByte(2) << 8));
            int off = (ip + ins.getLength() + rel) & 0xFFFF;
            SegmentedAddressSpace sp = (SegmentedAddressSpace) at.getAddressSpace();
            Address target = sp.getAddress(seg, off);

            Address[] flows = ins.getFlows();
            if (flows.length == 1 && flows[0].equals(target)) {
                if (isCall && getFunctionAt(target) == null) {
                    if (getInstructionAt(target) == null) disassemble(target);
                    if (new CreateFunctionCmd(target).applyTo(currentProgram, monitor)) created++;
                    else println("WARN: Funktion bei " + target + " (Aufruf " + at + ") nicht anlegbar");
                }
                continue;
            }

            for (Reference r : ins.getReferencesFrom())
                if (r.getReferenceType().isFlow()) rm.delete(r);
            RefType t = isCall ? RefType.CALL_OVERRIDE_UNCONDITIONAL : RefType.JUMP_OVERRIDE_UNCONDITIONAL;
            Reference nr = rm.addMemoryReference(ins.getAddress(), target, t, SourceType.USER_DEFINED, Reference.MNEMONIC);
            rm.setPrimary(nr, true);
            fixed++;

            if (isCall && getFunctionAt(target) == null) {
                if (getInstructionAt(target) == null) disassemble(target);
                if (new CreateFunctionCmd(target).applyTo(currentProgram, monitor)) created++;
            }
        }
        println("Near-Calls korrigiert: " + fixed + ", neue Funktionen: " + created);
        analyzeChanges(currentProgram);
    }
}
