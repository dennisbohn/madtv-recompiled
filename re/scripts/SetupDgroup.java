// Pre-Analyse für Turbo-C-Large-Model: DS zeigt in allen Code-Segmenten auf DGROUP.
// Das DGROUP-Segment wird als Daten (nicht ausführbar) markiert und umbenannt.
// Aufruf: analyzeHeadless ... -preScript SetupDgroup.java <dgroup-segment-hex>
//@category MadTV

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.ProgramContext;
import ghidra.program.model.mem.MemoryBlock;

import java.math.BigInteger;

public class SetupDgroup extends GhidraScript {
    @Override
    public void run() throws Exception {
        int dgroup = Integer.parseInt(getScriptArgs()[0], 16);
        ProgramContext ctx = currentProgram.getProgramContext();
        Register ds = currentProgram.getRegister("DS");

        for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
            if (!b.isInitialized() || b.getName().equals("HEADER")) continue;
            int seg = (int) (b.getStart().getOffset() >> 4);
            if (seg >= dgroup) {
                b.setExecute(false);
                if (seg == dgroup) b.setName("DGROUP");
                continue;
            }
            ctx.setValue(ds, b.getStart(), b.getEnd(), BigInteger.valueOf(dgroup));
        }
        println("DS=" + Integer.toHexString(dgroup) + " gesetzt");
    }
}
