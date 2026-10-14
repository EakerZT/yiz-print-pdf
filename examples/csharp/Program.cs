using System;
using System.IO;
using YizPrinting;
class Program
{
    static int Main(string[] args)
    {
        if (args.Length == 0) { Console.WriteLine("example PDF [printer] [emf-path | --print]"); return 2; }
        var options = PrintOptions.Create();
        options.Operation = Operation.Inspect;
        if (args.Length >= 3) {
            options.Printer = args[1]; options.ScaleMode = ScaleMode.Fit;
            options.Operation = args[2] == "--print" ? Operation.Print : Operation.Emf;
            if (options.Operation == Operation.Emf) options.EmfPath = args[2];
        }
        // Use your PDF generator's byte[] directly in production, without a temp file.
        var result = PrinterSdk.PrintMemory(File.ReadAllBytes(args[0]), options);
        Console.WriteLine(result.Json); Console.Error.Write(result.Logs);
        return result.Code;
    }
}
