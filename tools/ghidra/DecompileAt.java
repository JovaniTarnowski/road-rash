// Decompile functions at explicit addresses (creating them if Ghidra did not) and write <address>.c.
// Usage: -postScript DecompileAt.java <outdir> <hexaddr>...
//@category RoadRash
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import java.nio.file.*;

public class DecompileAt extends GhidraScript {
    public void run() throws Exception {
        String[] args = getScriptArgs();
        Path out = Paths.get(args[0]);
        FunctionManager fm = currentProgram.getFunctionManager();
        DecompInterface dec = new DecompInterface();
        dec.openProgram(currentProgram);
        try {
            for (int i = 1; i < args.length; ++i) {
                Address t = toAddr(Long.parseLong(args[i], 16));
                if (currentProgram.getListing().getInstructionAt(t) == null) disassemble(t);
                Function f = fm.getFunctionAt(t);
                if (f == null) f = createFunction(t, null);
                if (f == null) { println("no function at " + t); continue; }
                DecompileResults r = dec.decompileFunction(f, 120, monitor);
                boolean ok = r.decompileCompleted() && r.getDecompiledFunction() != null;
                Files.writeString(out.resolve(t.toString() + ".c"), ok ? r.getDecompiledFunction().getC() : "/* " + r.getErrorMessage() + " */");
                println("exported " + t + " ok=" + ok);
            }
        } finally { dec.dispose(); }
    }
}
