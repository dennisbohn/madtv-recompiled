// Legt Funktionen an den übergebenen Adressen (seg:off) an, z.B. Handler, die nur
// über Zeigertabellen erreichbar sind.
//@category MadTV

import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class CreateFuncs extends GhidraScript {
    @Override
    public void run() throws Exception {
        for (String arg : getScriptArgs()) {
            Address a = toAddr(arg);
            Function old = getFunctionContaining(a);
            if (old != null && !old.getEntryPoint().equals(a)) {
                // Einschluss in fremde Funktion auflösen: alte Funktion danach neu berechnen
                println("liegt in " + old.getName() + ", wird abgetrennt");
            }
            if (getInstructionAt(a) == null) disassemble(a);
            boolean ok = getFunctionAt(a) != null || new CreateFunctionCmd(a).applyTo(currentProgram, monitor);
            if (old != null && !old.getEntryPoint().equals(a))
                CreateFunctionCmd.fixupFunctionBody(currentProgram, old, monitor);
            println(arg + ": " + (ok ? "ok" : "FEHLER"));
        }
    }
}
