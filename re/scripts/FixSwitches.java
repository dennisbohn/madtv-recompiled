// Repariert Turbo-C-switch-Anweisungen, die Ghidra im 16-Bit-Modus nicht erkennt.
// Muster A (Index):  CMP AX,N ; JA/JBE ... ; MOV BX,AX ; SHL BX,1 ; JMP CS:[BX+tab]
// Muster B (Werte):  MOV CX,N ; MOV BX,vals ; CMP AX,CS:[BX] ; JZ ; INC BX x2 ; LOOP ;
//                    JMP default ; JMP CS:[BX+2N]     (Ziele liegen direkt hinter den Werten)
// Wiederholt sich, bis keine neuen Sprünge mehr gefunden werden (Ziele enthalten weitere switches).
//@category MadTV

import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.data.WordDataType;
import ghidra.program.model.listing.*;
import ghidra.program.model.pcode.JumpTable;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.*;

import java.util.*;

public class FixSwitches extends GhidraScript {

    private final Set<Address> done = new HashSet<>();

    @Override
    public void run() throws Exception {
        int total = 0;
        for (int pass = 0; pass < 10; pass++) {
            int n = onePass();
            total += n;
            println("Pass " + pass + ": " + n + " switches");
            if (n == 0) break;
        }
        println("switches repariert: " + total);
    }

    private int onePass() throws Exception {
        List<Instruction> jumps = new ArrayList<>();
        for (Instruction ins : currentProgram.getListing().getInstructions(true))
            if (ins.getMnemonicString().equals("JMP") && ins.toString().contains("CS:[")
                    && !done.contains(ins.getAddress()))
                jumps.add(ins);

        println("Kandidaten: " + jumps.size());
        int fixed = 0;
        for (Instruction jmp : jumps) {
            SegmentedAddress at = (SegmentedAddress) jmp.getAddress();
            SegmentedAddressSpace sp = (SegmentedAddressSpace) at.getAddressSpace();
            int seg = at.getSegment();
            // Kodierung: 2E FF A7 lo hi  =  JMP word ptr CS:[BX + disp16]
            //            2E FF 67 dd     =  JMP word ptr CS:[BX + disp8]
            byte[] b = jmp.getBytes();
            int disp;
            if (b.length == 5 && b[0] == (byte) 0x2E && b[1] == (byte) 0xFF && b[2] == (byte) 0xA7)
                disp = (b[3] & 0xff) | ((b[4] & 0xff) << 8);
            else if (b.length == 4 && b[0] == (byte) 0x2E && b[1] == (byte) 0xFF && b[2] == (byte) 0x67)
                disp = b[3];   // vorzeichenbehaftet
            else {
                println("WARN: unbekannte Kodierung bei " + at + ": " + jmp);
                done.add(at);
                continue;
            }

            int count = -1, tableOff = -1, valuesOff = -1;
            Instruction p = jmp.getPrevious();
            for (int i = 0; i < 10 && p != null; i++, p = p.getPrevious()) {
                String m = p.getMnemonicString();
                String op0 = p.getDefaultOperandRepresentation(0);
                Scalar s1 = p.getScalar(1);
                if (m.equals("CMP") && s1 != null && count < 0 && valuesOff < 0 && !p.toString().contains("CS:")) {
                    count = (int) s1.getUnsignedValue() + 1;       // Muster A
                    tableOff = disp;
                    break;
                }
                if (m.equals("MOV") && op0.equals("BX") && s1 != null) valuesOff = (int) s1.getUnsignedValue();
                if (m.equals("MOV") && op0.equals("CX") && s1 != null && valuesOff >= 0) {
                    count = (int) s1.getUnsignedValue();            // Muster B
                    tableOff = valuesOff + disp;
                    break;
                }
            }
            if (count <= 0 || count > 300) {
                println("WARN: Muster nicht erkannt bei " + at);
                continue;
            }

            Address tab = sp.getAddress(seg, tableOff);
            Address dataStart = valuesOff >= 0 ? sp.getAddress(seg, valuesOff) : tab;
            Address dataEnd = tab.add(count * 2L - 1);
            clearListing(dataStart, dataEnd);

            ArrayList<Address> dests = new ArrayList<>();
            for (int i = 0; i < count; i++) {
                Address e = tab.add(i * 2L);
                int off = (getByte(e) & 0xff) | ((getByte(e.add(1)) & 0xff) << 8);
                dests.add(sp.getAddress(seg, off));
            }
            for (Address a = dataStart; a.compareTo(dataEnd) < 0; a = a.add(2))
                createData(a, WordDataType.dataType);

            ReferenceManager rm = currentProgram.getReferenceManager();
            for (Address d : new LinkedHashSet<>(dests)) {
                if (getInstructionAt(d) == null) {
                    clearListing(d, d);
                    disassemble(d);
                }
                rm.addMemoryReference(at, d, RefType.COMPUTED_JUMP, SourceType.USER_DEFINED, 0);
            }
            done.add(at);

            fixed++;
            // Die kaputte Tabelle hat oft die Funktionsgrenze abgeschnitten:
            // dann gehört der Sprung zur nächstvorherigen Funktion im selben Segment.
            Function f = getFunctionContaining(at);
            if (f == null) {
                Function before = getFunctionBefore(at);
                if (before != null && ((SegmentedAddress) before.getEntryPoint()).getSegment() == seg)
                    f = before;
            }
            if (f == null) {
                println("WARN: keine Funktion für switch bei " + at);
                continue;
            }
            CreateFunctionCmd.fixupFunctionBody(currentProgram, f, monitor);
            if (f.getBody().contains(at))
                new JumpTable(at, dests, true, 0).writeOverride(f);
            else
                println("WARN: " + f.getName() + " enthält switch " + at + " nicht");
        }
        return fixed;
    }
}
