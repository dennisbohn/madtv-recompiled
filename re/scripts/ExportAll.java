// Exportiert Analyseergebnisse von MTV.EXE: Funktionsliste, dekompilierten C-Code,
// Strings, Segmente und Software-Interrupt-Aufrufe (INT xx mit AH-Wert).
// Aufruf über analyzeHeadless ... -postScript ExportAll.java <Ausgabeordner>
//@category MadTV

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.scalar.Scalar;

import java.io.File;
import java.io.PrintWriter;
import java.util.*;

public class ExportAll extends GhidraScript {

    @Override
    public void run() throws Exception {
        File outDir = new File(getScriptArgs()[0]);
        outDir.mkdirs();
        Listing listing = currentProgram.getListing();
        FunctionManager fm = currentProgram.getFunctionManager();

        try (PrintWriter w = new PrintWriter(new File(outDir, "segments.txt"), "UTF-8")) {
            for (MemoryBlock b : currentProgram.getMemory().getBlocks())
                w.printf("%-12s %s - %s  %6d  %s%s%s%n", b.getName(), b.getStart(), b.getEnd(), b.getSize(),
                        b.isRead() ? "r" : "-", b.isWrite() ? "w" : "-", b.isExecute() ? "x" : "-");
        }

        try (PrintWriter w = new PrintWriter(new File(outDir, "functions.tsv"), "UTF-8")) {
            w.println("entry\tname\tsize\tcallers\tcallees");
            for (Function f : fm.getFunctions(true)) {
                w.printf("%s\t%s\t%d\t%d\t%d%n", f.getEntryPoint(), f.getName(), f.getBody().getNumAddresses(),
                        f.getCallingFunctions(monitor).size(), f.getCalledFunctions(monitor).size());
            }
        }

        try (PrintWriter w = new PrintWriter(new File(outDir, "strings.tsv"), "UTF-8")) {
            for (Data d : listing.getDefinedData(true)) {
                if (d.hasStringValue())
                    w.printf("%s\t%s%n", d.getAddress(), String.valueOf(d.getValue()).replace("\n", "\\n").replace("\r", "\\r"));
            }
        }

        // INT-Instruktionen mit dem zuletzt gesetzten AH/AX-Wert im selben Basisblock (Heuristik).
        try (PrintWriter w = new PrintWriter(new File(outDir, "interrupts.tsv"), "UTF-8")) {
            w.println("address\tfunction\tint\tah_or_ax");
            for (Instruction ins : listing.getInstructions(true)) {
                if (!ins.getMnemonicString().equalsIgnoreCase("INT")) continue;
                Scalar vec = ins.getScalar(0);
                String reg = "?";
                Instruction prev = ins.getPrevious();
                for (int i = 0; i < 8 && prev != null; i++, prev = prev.getPrevious()) {
                    String m = prev.getMnemonicString();
                    String op0 = prev.getDefaultOperandRepresentation(0);
                    if (m.equalsIgnoreCase("MOV") && (op0.equalsIgnoreCase("AH") || op0.equalsIgnoreCase("AX"))) {
                        reg = op0 + "=" + prev.getDefaultOperandRepresentation(1);
                        break;
                    }
                }
                Function f = fm.getFunctionContaining(ins.getAddress());
                w.printf("%s\t%s\t%s\t%s%n", ins.getAddress(), f == null ? "-" : f.getName(),
                        vec == null ? "?" : String.format("%02Xh", vec.getUnsignedValue()), reg);
            }
        }

        // Port-I/O (IN/OUT) – Hinweise auf VGA-, Timer-, Sound-Hardwarezugriffe.
        try (PrintWriter w = new PrintWriter(new File(outDir, "portio.tsv"), "UTF-8")) {
            for (Instruction ins : listing.getInstructions(true)) {
                String m = ins.getMnemonicString().toUpperCase();
                if (!(m.startsWith("IN") && !m.equals("INC") && !m.equals("INT") || m.startsWith("OUT"))) continue;
                Function f = fm.getFunctionContaining(ins.getAddress());
                w.printf("%s\t%s\t%s%n", ins.getAddress(), f == null ? "-" : f.getName(), ins);
            }
        }

        DecompInterface dec = new DecompInterface();
        dec.openProgram(currentProgram);
        // Fallback: Ghidra stürzt bei manchen Far-Pointer-Konstanten (seg:off als 32 Bit) ab,
        // wenn es sie als Zeiger interpretiert. Ohne Zeiger-Inferenz klappt es.
        DecompInterface decNoPtr = new DecompInterface();
        DecompileOptions noPtr = new DecompileOptions();
        noPtr.setInferConstantPointers(false);
        decNoPtr.setOptions(noPtr);
        decNoPtr.openProgram(currentProgram);
        int ok = 0, fail = 0;
        try (PrintWriter w = new PrintWriter(new File(outDir, "decompiled.c"), "UTF-8")) {
            for (Function f : fm.getFunctions(true)) {
                if (monitor.isCancelled()) break;
                DecompileResults r = dec.decompileFunction(f, 60, monitor);
                if (r == null || !r.decompileCompleted()) {
                    // Code wie MK_FP(DS, 0x831e): mit bekanntem DS faltet der Decompiler
                    // seg:off zu einer ungültigen linearen Adresse. Ohne DS-Wert klappt es.
                    var ctx = currentProgram.getProgramContext();
                    var ds = currentProgram.getRegister("DS");
                    for (var rng : f.getBody()) ctx.remove(rng.getMinAddress(), rng.getMaxAddress(), ds);
                    DecompInterface d2 = new DecompInterface();
                    d2.setOptions(noPtr);
                    d2.openProgram(currentProgram);
                    r = d2.decompileFunction(f, 60, monitor);
                    d2.dispose();
                    if (r != null && r.decompileCompleted())
                        w.printf("%n// HINWEIS: ohne DS-Kontext dekompiliert (DS == DGROUP)");
                }
                w.printf("%n// ===== %s @ %s =====%n", f.getName(), f.getEntryPoint());
                if (r != null && r.decompileCompleted()) {
                    w.println(r.getDecompiledFunction().getC());
                    ok++;
                } else {
                    w.println("// decompile failed: " + (r == null ? "null" : r.getErrorMessage()));
                    fail++;
                }
            }
        }
        dec.dispose();
        decNoPtr.dispose();
        println("Functions decompiled: " + ok + ", failed: " + fail);
    }
}
