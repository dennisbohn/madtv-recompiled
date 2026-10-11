// Exportiert Signaturen aller Funktionen: Rückgabetyp/-größe und Parameter samt Stack-Offset.
// Zusätzlich: höchster [BP+n]-Zugriff im Rumpf (Argumentbereich ab BP+6 bei far-Funktionen),
// als Gegenprobe zur Ghidra-Parametererkennung.
// Aufruf: -postScript ExportSigs.java <ausgabe.tsv>
//@category MadTV

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;

import java.io.File;
import java.io.PrintWriter;
import java.util.regex.*;

public class ExportSigs extends GhidraScript {
    private static final Pattern BP = Pattern.compile("\\[BP \\+ 0x([0-9a-f]+)\\]");

    @Override
    public void run() throws Exception {
        try (PrintWriter w = new PrintWriter(new File(getScriptArgs()[0]), "UTF-8")) {
            w.println("entry\tname\tconv\tret\tretsize\tparams\tmax_bp_arg\tfar\tsize");
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                StringBuilder ps = new StringBuilder();
                for (Parameter p : f.getParameters()) {
                    if (ps.length() > 0) ps.append(";");
                    String stor = p.isStackVariable() ? "s" + p.getStackOffset() : p.getVariableStorage().toString();
                    ps.append(p.getDataType().getName()).append(":").append(p.getLength()).append(":").append(stor);
                }
                int maxBp = 0;
                boolean far = false;
                for (Instruction ins : currentProgram.getListing().getInstructions(f.getBody(), true)) {
                    String m = ins.getMnemonicString();
                    if (m.equals("RETF")) far = true;
                    Matcher mm = BP.matcher(ins.toString());
                    while (mm.find()) {
                        int off = Integer.parseInt(mm.group(1), 16);
                        if (off < 0x80) maxBp = Math.max(maxBp, off);
                    }
                }
                w.printf("%s\t%s\t%s\t%s\t%d\t%s\t%d\t%d\t%d%n", f.getEntryPoint(), f.getName(),
                        f.getCallingConventionName(), f.getReturnType().getName(), f.getReturnType().getLength(),
                        ps, maxBp, far ? 1 : 0, f.getBody().getNumAddresses());
            }
        }
    }
}
