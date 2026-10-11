// Listet alle indirekten Sprünge über CS-relative Tabellen (Turbo-C-switch) mit Kontext.
//@category MadTV

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Instruction;

public class FindSwitches extends GhidraScript {
    @Override
    public void run() throws Exception {
        for (Instruction ins : currentProgram.getListing().getInstructions(true)) {
            if (!ins.getMnemonicString().equals("JMP")) continue;
            String s = ins.toString();
            if (!s.contains("CS:[")) continue;
            StringBuilder sb = new StringBuilder();
            Instruction p = ins;
            for (int i = 0; i < 8 && p.getPrevious() != null; i++) p = p.getPrevious();
            for (; p != null && !p.getAddress().equals(ins.getNext().getAddress()); p = p.getNext())
                sb.append("   ").append(p.getAddress()).append("  ").append(p).append("\n");
            println("SWITCH " + ins.getAddress() + "\n" + sb);
        }
    }
}
