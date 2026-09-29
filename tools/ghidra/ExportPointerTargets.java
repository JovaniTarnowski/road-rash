// Create and export functions reached only through code pointers stored in .data
// (behaviour tables such as 0x00463ba8). Writes <address>.c next to the main export
// and appends rows to functions-extra.tsv.
//@category RoadRash
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;

public class ExportPointerTargets extends GhidraScript {
    public void run() throws Exception {
        Path out = Paths.get(getScriptArgs()[0]);
        Memory mem = currentProgram.getMemory();
        MemoryBlock text = mem.getBlock(".text"), data = mem.getBlock(".data");
        Listing listing = currentProgram.getListing();
        FunctionManager fm = currentProgram.getFunctionManager();
        TreeSet<Address> targets = new TreeSet<>();
        Address a = data.getStart();
        long initialized = data.getStart().getOffset() + 0x16400; // raw file size of .data; the rest is BSS
        while (a.getOffset() + 4 <= initialized && a.compareTo(data.getEnd()) < 0) {
            long v = mem.getInt(a) & 0xffffffffL;
            if (v >= text.getStart().getOffset() && v <= text.getEnd().getOffset()) {
                Address t = text.getStart().getNewAddress(v);
                if (fm.getFunctionAt(t) == null && fm.getFunctionContaining(t) == null) targets.add(t);
            }
            a = a.add(4);
        }
        // Keep only targets that decode as instructions (disassemble on demand).
        DecompInterface dec = new DecompInterface();
        dec.openProgram(currentProgram);
        int made = 0;
        try (PrintWriter index = new PrintWriter(Files.newBufferedWriter(out.resolve("functions-extra.tsv"), StandardCharsets.UTF_8))) {
            index.println("address\tname\tbytes\tdecompiled");
            for (Address t : targets) {
                if (fm.getFunctionContaining(t) != null) continue;
                if (listing.getInstructionAt(t) == null) disassemble(t);
                if (listing.getInstructionAt(t) == null) continue;
                Function f = createFunction(t, null);
                if (f == null) continue;
                DecompileResults r = dec.decompileFunction(f, 60, monitor);
                boolean ok = r.decompileCompleted() && r.getDecompiledFunction() != null;
                Files.writeString(out.resolve(t.toString() + ".c"), ok ? r.getDecompiledFunction().getC() : "/* " + r.getErrorMessage() + " */");
                index.printf("%s\t%s\t%d\t%b%n", t, f.getName(), f.getBody().getNumAddresses(), ok);
                made++;
            }
        } finally { dec.dispose(); }
        println("Created and exported " + made + " pointer-target functions");
    }
}
