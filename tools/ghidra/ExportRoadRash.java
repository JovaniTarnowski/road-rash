// Export address-keyed decompilation, functions and interesting string references.
//@category RoadRash
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.Reference;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;

public class ExportRoadRash extends GhidraScript {
    public void run() throws Exception {
        Path out = Paths.get(getScriptArgs()[0]);
        Files.createDirectories(out);
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        int count = 0, failed = 0;
        try (PrintWriter index = new PrintWriter(Files.newBufferedWriter(out.resolve("functions.tsv"), StandardCharsets.UTF_8));
             PrintWriter refs = new PrintWriter(Files.newBufferedWriter(out.resolve("string-references.tsv"), StandardCharsets.UTF_8))) {
            index.println("address\tname\tbytes\tdecompiled");
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                if (monitor.isCancelled()) break;
                if (f.isExternal()) continue;
                DecompileResults result = decompiler.decompileFunction(f, 30, monitor);
                boolean ok = result.decompileCompleted() && result.getDecompiledFunction() != null;
                String body = ok ? result.getDecompiledFunction().getC() : "/* " + result.getErrorMessage() + " */";
                Files.writeString(out.resolve(f.getEntryPoint().toString() + ".c"), body);
                index.printf("%s\t%s\t%d\t%b%n", f.getEntryPoint(), f.getName(), f.getBody().getNumAddresses(), ok);
                count++;
                if (!ok) failed++;
            }
            refs.println("string_address\treference_address\tfunction\tvalue");
            for (Data d : currentProgram.getListing().getDefinedData(true)) {
                if (!d.hasStringValue()) continue;
                String value = String.valueOf(d.getValue()).replace('\t', ' ').replace('\n', ' ').replace('\r', ' ');
                for (Reference r : getReferencesTo(d.getAddress())) {
                    Function f = getFunctionContaining(r.getFromAddress());
                    refs.printf("%s\t%s\t%s\t%s%n", d.getAddress(), r.getFromAddress(), f == null ? "" : f.getEntryPoint(), value);
                }
            }
        } finally { decompiler.dispose(); }
        println("Exported " + count + " functions; failed: " + failed);
    }
}
