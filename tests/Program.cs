using System;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using System.Text.Json;
using YizPrinting;
class Smoke
{
    static void Check(bool value, string message) { if (!value) throw new Exception(message); }
    static void Main(string[] args)
    {
        var bytes = File.ReadAllBytes(args[0]);
        var options = PrintOptions.Create(); options.Operation = Operation.Inspect;
        Check(PrinterSdk.YizGetAbiVersion() == 1, "ABI");
        var file = PrinterSdk.PrintFile(args[0], options);
        var memory = PrinterSdk.PrintMemory(bytes, options);
        Check(file.Code == 0 && memory.Code == 0 && file.Json == memory.Json, "file/memory parity");
        options.AbiVersion = 99;
        Check(PrinterSdk.PrintMemory(bytes, options).Code == 2, "ABI rejection");
        options.AbiVersion = 1; options.ScaleMode = (ScaleMode)99;
        Check(PrinterSdk.PrintMemory(bytes, options).Code == 2, "enum rejection");
        options.ScaleMode = ScaleMode.Percent;
        Check(PrinterSdk.PrintMemory(new byte[]{1,2,3}, options).Code == 3, "bad PDF");
        Task.WaitAll(Enumerable.Range(0, 12).Select(i => Task.Run(() => {
            var r = PrinterSdk.PrintMemory(bytes, options);
            Check(r.Code == 0 && r.Json == file.Json, "concurrent/repeated memory calls");
        })).ToArray());
        Check(PrinterSdk.ListPrinters().Code == 0, "enumeration");
        var batch = PrinterSdk.PrintMemoryBatch(new[] { bytes, bytes }, options);
        var fileBatch = PrinterSdk.PrintFiles(new[] { args[0], args[0] }, options);
        Check(batch.Code == 0 && fileBatch.Code == 0 && batch.Json == fileBatch.Json, "batch file/memory parity");
        using (var parsed = JsonDocument.Parse(batch.Json)) {
            Check(parsed.RootElement.GetProperty("completed").GetInt32() == 2, "batch success count");
            Check(parsed.RootElement.GetProperty("failedIndex").GetInt32() == -1, "batch success index");
        }
        var partial = PrinterSdk.PrintMemoryBatch(new[] { bytes, new byte[] {1,2,3}, bytes }, options);
        Check(partial.Code == 3, "batch PDF failure");
        using (var parsed = JsonDocument.Parse(partial.Json)) {
            var root = parsed.RootElement;
            Check(root.GetProperty("completed").GetInt32() == 1 && root.GetProperty("attempted").GetInt32() == 2, "stop after failure");
            Check(root.GetProperty("failedIndex").GetInt32() == 1, "failure index");
            Check(root.GetProperty("items")[2].GetProperty("status").GetString() == "skipped", "skip remaining");
        }
        var missing = PrinterSdk.PrintFiles(new[] { args[0], Path.Combine(Path.GetTempPath(), Guid.NewGuid() + ".pdf"), args[0] }, options);
        Check(missing.Code == 3 && missing.Json.Contains("\"failedIndex\":1"), "missing batch file");
        options.Operation = Operation.Emf;
        Check(PrinterSdk.PrintMemoryBatch(new[] { bytes }, options).Code == 2, "reject batch EMF");
        options.Operation = Operation.Inspect;
        bool rejected = false;
        try { PrinterSdk.PrintMemoryBatch(new byte[0][], options); } catch (ArgumentOutOfRangeException) { rejected = true; }
        Check(rejected, "empty batch rejection");
        Task.WaitAll(Enumerable.Range(0, 4).Select(i => Task.Run(() => {
            Check(PrinterSdk.PrintMemoryBatch(new[] {bytes, bytes}, options).Code == 0, "concurrent batch");
        })).ToArray());
        Console.WriteLine("PASS: batch parity, partial failure, missing file, skipped items, rejected EMF/empty batch, concurrent batch");
        Console.WriteLine("PASS: ABI, file/memory parity, invalid input, repeated/concurrent calls, enumeration (no printing)");
    }
}
