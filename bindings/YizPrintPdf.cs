using System;
using System.Runtime.InteropServices;
using System.Text;

namespace YizPrinting
{
    public enum ScaleMode { Percent, Fit, Shrink }
    public enum PaperMode { Driver, Pdf }
    public enum TextMode { Alternate, Original }
    public enum Operation { Print, Inspect, Emf }

    // ABI v1, Pack=8; fields use explicit 32-bit integers (no native bool).
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode, Pack = 8)]
    public struct PrintOptions
    {
        public uint StructSize, AbiVersion;
        [MarshalAs(UnmanagedType.LPWStr)] public string Printer;
        [MarshalAs(UnmanagedType.LPWStr)] public string EmfPath;
        public ScaleMode ScaleMode;
        public PaperMode PaperMode;
        public TextMode TextMode;
        public Operation Operation;
        public int Center, RequireAlternate;
        public double ScalePercent, WidthMm, HeightMm, OffsetXMm, OffsetYMm;
        public static PrintOptions Create(string printer = null)
        {
            return new PrintOptions {
                StructSize = (uint)Marshal.SizeOf(typeof(PrintOptions)), AbiVersion = 1,
                Printer = printer, ScalePercent = 100
            };
        }
    }
    public sealed class PrintResult
    {
        public int Code { get; private set; }
        public string Json { get; private set; }
        public string Logs { get; private set; }
        internal PrintResult(int code, string json, string logs) { Code = code; Json = json; Logs = logs; }
    }
    public static class PrinterSdk
    {
        private const string Dll = "yiz-print-pdf.dll";
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        public static extern uint YizGetAbiVersion();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, CharSet = CharSet.Unicode)]
        private static extern int YizPrintFile(string path, ref PrintOptions options, out IntPtr result);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        private static extern int YizPrintMemory(IntPtr bytes, UIntPtr length, ref PrintOptions options, out IntPtr result);
        [StructLayout(LayoutKind.Sequential, Pack = 8)]
        private struct PdfBuffer { public IntPtr Data; public UIntPtr Length; }
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        private static extern int YizPrintFiles([In] IntPtr[] paths, uint count, ref PrintOptions options, out IntPtr result);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        private static extern int YizPrintMemoryBatch([In] PdfBuffer[] buffers, uint count, ref PrintOptions options, out IntPtr result);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        private static extern int YizListPrinters(out IntPtr result);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        private static extern IntPtr YizResultJson(IntPtr result);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        private static extern IntPtr YizResultLogs(IntPtr result);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        private static extern void YizFreeResult(IntPtr result);
        private static string Utf8(IntPtr ptr)
        {
            if (ptr == IntPtr.Zero) return "";
            int length = 0;
            while (Marshal.ReadByte(ptr, length) != 0) length++;
            byte[] bytes = new byte[length];
            Marshal.Copy(ptr, bytes, 0, length);
            return Encoding.UTF8.GetString(bytes);
        }
        private static PrintResult Read(int code, IntPtr result)
        {
            try { return new PrintResult(code, Utf8(YizResultJson(result)), Utf8(YizResultLogs(result))); }
            finally { YizFreeResult(result); }
        }
        // Blocking API: call on a background thread in UI applications.
        public static PrintResult PrintFile(string path, PrintOptions options)
        {
            IntPtr result;
            int code = YizPrintFile(path, ref options, out result);
            return Read(code, result);
        }
        public static PrintResult PrintMemory(byte[] pdf, PrintOptions options)
        {
            if (pdf == null) throw new ArgumentNullException("pdf");
            if (pdf.Length == 0 || pdf.Length > 512 * 1024 * 1024) throw new ArgumentOutOfRangeException("pdf");
            var pinned = GCHandle.Alloc(pdf, GCHandleType.Pinned);
            try {
                IntPtr result;
                int code = YizPrintMemory(pinned.AddrOfPinnedObject(), new UIntPtr((uint)pdf.Length), ref options, out result);
                return Read(code, result);
            } finally { pinned.Free(); }
        }
        // One independent job per PDF, sequentially; stop on first failure.
        // Inspect is supported for dry checks; Emf is not supported in batches.
        public static PrintResult PrintFiles(string[] paths, PrintOptions options)
        {
            if (paths == null) throw new ArgumentNullException("paths");
            if (paths.Length == 0 || paths.Length > 100) throw new ArgumentOutOfRangeException("paths");
            var pointers = new IntPtr[paths.Length];
            try {
                for (int i = 0; i < paths.Length; i++) {
                    if (string.IsNullOrEmpty(paths[i]) || paths[i].IndexOf('\0') >= 0)
                        throw new ArgumentException("Paths must be nonempty and contain no NUL characters.", "paths");
                    pointers[i] = Marshal.StringToHGlobalUni(paths[i]);
                }
                IntPtr result;
                int code = YizPrintFiles(pointers, (uint)pointers.Length, ref options, out result);
                return Read(code, result);
            } finally { foreach (var pointer in pointers) if (pointer != IntPtr.Zero) Marshal.FreeHGlobal(pointer); }
        }
        public static PrintResult PrintMemoryBatch(byte[][] pdfs, PrintOptions options)
        {
            if (pdfs == null) throw new ArgumentNullException("pdfs");
            if (pdfs.Length == 0 || pdfs.Length > 100) throw new ArgumentOutOfRangeException("pdfs");
            var buffers = new PdfBuffer[pdfs.Length];
            var pins = new GCHandle[pdfs.Length];
            long total = 0;
            try {
                for (int i = 0; i < pdfs.Length; i++) {
                    var bytes = pdfs[i];
                    if (bytes == null || bytes.Length == 0) throw new ArgumentException("Empty PDF buffer.", "pdfs");
                    total += bytes.Length;
                    if (total > 512L * 1024 * 1024) throw new ArgumentOutOfRangeException("pdfs", "Batch exceeds 512 MiB.");
                    pins[i] = GCHandle.Alloc(bytes, GCHandleType.Pinned);
                    buffers[i] = new PdfBuffer { Data = pins[i].AddrOfPinnedObject(), Length = new UIntPtr((uint)bytes.Length) };
                }
                IntPtr result;
                int code = YizPrintMemoryBatch(buffers, (uint)buffers.Length, ref options, out result);
                return Read(code, result);
            } finally { foreach (var pin in pins) if (pin.IsAllocated) pin.Free(); }
        }
        public static PrintResult ListPrinters()
        {
            IntPtr result;
            int code = YizListPrinters(out result);
            return Read(code, result);
        }
    }
}
