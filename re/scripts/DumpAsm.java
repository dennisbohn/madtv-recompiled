// Gibt Disassembly für Adressbereiche aus: Argumente "seg:off:anzahl" ...
//@category MadTV

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;

public class DumpAsm extends GhidraScript {
    @Override
    public void run() throws Exception {
        for (String arg : getScriptArgs()) {
            String[] p = arg.split(":");
            Address a = toAddr(p[0] + ":" + p[1]);
            int n = Integer.parseInt(p[2]);
            println("---- " + a);
            Instruction ins = getInstructionAt(a);
            if (ins == null) ins = getInstructionAfter(a);
            for (int i = 0; i < n && ins != null; i++, ins = ins.getNext()) {
                StringBuilder refs = new StringBuilder();
                for (var r : ins.getReferencesFrom()) refs.append(" -> ").append(r.getToAddress());
                println(ins.getAddress() + "  " + ins + refs);
            }
        }
    }
}
