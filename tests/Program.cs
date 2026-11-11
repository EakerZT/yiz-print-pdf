using System;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using System.Text.Json;
using System.Runtime.InteropServices;
using System.Text.RegularExpressions;
using System.Globalization;
using YizPrinting;
class Smoke
{
    static void Check(bool value, string message) { if (!value) throw new Exception(message); }
    [DllImport("yiz-print-pdf.dll", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    static extern int YizInitOptions(ref PrintOptions options, uint size);
    static void Main(string[] args)
    {
        var bytes = File.ReadAllBytes(args[0]);
        Check(PrintOptions.Create().TextMode == TextMode.Original, "default original text mode");
        var initialized = new PrintOptions();
        Check(YizInitOptions(ref initialized, (uint)Marshal.SizeOf<PrintOptions>()) == 0 &&
              initialized.AbiVersion == 2 && initialized.Orientation == Orientation.Driver, "native ABI v2 defaults and structure layout");
        var options = PrintOptions.Create(); options.Operation = Operation.Inspect;
        Check(PrinterSdk.YizGetAbiVersion() == 2, "ABI v2");
        var file = PrinterSdk.PrintFile(args[0], options);
        var memory = PrinterSdk.PrintMemory(bytes, options);
        Check(file.Code == 0 && memory.Code == 0 && file.Json == memory.Json, "file/memory parity");
        options.AbiVersion = 1;
        Check(PrinterSdk.PrintMemory(bytes, options).Code == 2, "ABI rejection");
        options.AbiVersion = 2;
        options.StructSize -= 8;
        Check(PrinterSdk.PrintMemory(bytes, options).Code == 2, "old structure size rejection");
        options.StructSize += 8;
        foreach (var invalid in new[] { -1, 3 }) {
            options.Orientation = (Orientation)invalid;
            Check(PrinterSdk.PrintFile(args[0], options).Code == 2 &&
                  PrinterSdk.PrintMemory(bytes, options).Code == 2 &&
                  PrinterSdk.PrintFiles(new[] { args[0] }, options).Code == 2 &&
                  PrinterSdk.PrintMemoryBatch(new[] { bytes }, options).Code == 2, "invalid orientation rejected by all input APIs");
        }
        foreach (Orientation orientation in Enum.GetValues(typeof(Orientation))) {
            options.Orientation = orientation;
            Check(PrinterSdk.PrintFile(args[0], options).Json == file.Json &&
                  PrinterSdk.PrintMemory(bytes, options).Json == file.Json &&
                  PrinterSdk.PrintMemoryBatch(new[] { bytes }, options).Code == 0,
                  "inspect preserves PDF dimensions for " + orientation);
        }
        options.Orientation = Orientation.Driver;
        options.ScaleMode = (ScaleMode)99;
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
        // Optional non-printing regression: use a PDF whose text must all take
        // the alternate path (e.g. non-embedded MicrosoftYaHei backed by TTC).
        if (args.Length > 1) {
            var emf = Path.Combine(Path.GetTempPath(), "yiz-font-check-" + Guid.NewGuid() + ".emf");
            try {
                // Render with a printer DC only: EMF never calls StartDoc.
                foreach (PaperMode paper in Enum.GetValues(typeof(PaperMode))) {
                    foreach (bool fixedSize in new[] { false, true }) {
                        foreach (Orientation orientation in Enum.GetValues(typeof(Orientation))) {
                            options = PrintOptions.Create(args[1]);
                            options.Operation = Operation.Emf;
                            options.EmfPath = emf;
                            options.PaperMode = paper;
                            options.Orientation = orientation;
                            options.ScaleMode = ScaleMode.Fit;
                            if (fixedSize) { options.WidthMm = 80; options.HeightMm = 120; }
                            var rendered = PrinterSdk.PrintMemory(bytes, options);
                            Check(rendered.Code == 0, "orientation EMF rendering: " + rendered.Json);
                            Check(new FileInfo(emf).Length > 0, "orientation EMF output");
                            var size = Regex.Match(rendered.Logs, @"physical=([\d.]+)x([\d.]+)mm");
                            Check(size.Success, "driver dimensions logged");
                            double width = double.Parse(size.Groups[1].Value, CultureInfo.InvariantCulture);
                            double height = double.Parse(size.Groups[2].Value, CultureInfo.InvariantCulture);
                            if (orientation != Orientation.Driver)
                                Check(orientation == Orientation.Landscape ? width > height : width < height,
                                      "driver applies " + orientation + " with " + paper + ", fixed=" + fixedSize);
                            var fromFile = PrinterSdk.PrintFile(args[0], options);
                            Check(fromFile.Code == 0 && fromFile.Json == rendered.Json && fromFile.Logs == rendered.Logs,
                                  "file/memory orientation rendering parity");
                        }
                    }
                }
                Console.WriteLine("PASS: driver/portrait/landscape via EMF, driver/PDF/fixed paper, file/memory parity (no printing)");
                options = PrintOptions.Create();
                options.Operation = Operation.Emf;
                options.Printer = args[1];
                options.EmfPath = emf;
                options.TextMode = TextMode.Alternate;
                options.RequireAlternate = 1;
                for (int i = 0; i < 2; ++i) {
                    var rendered = PrinterSdk.PrintMemory(bytes, options);
                    Check(rendered.Code == 0, "strict alternate memory rendering: " + rendered.Json);
                    using var json = JsonDocument.Parse(rendered.Json);
                    Check(json.RootElement.GetProperty("alternateGlyphs").GetInt32() > 0 &&
                          json.RootElement.GetProperty("fallbackRuns").GetInt32() == 0, "no font fallback");
                    Check(new FileInfo(emf).Length > 0, "EMF output");
                }
                Console.WriteLine("PASS: repeated strict alternate memory rendering, zero fallbacks (no printing)");
            } finally { if (File.Exists(emf)) File.Delete(emf); }
        }
    }
}
