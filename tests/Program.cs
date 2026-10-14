using System;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
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
        Console.WriteLine("PASS: ABI, file/memory parity, invalid input, repeated/concurrent calls, enumeration (no printing)");
    }
}
