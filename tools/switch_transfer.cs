// Windows Shell transfer adapter. IFileOperation also supports MTP namespaces.
using System;
using System.Runtime.InteropServices;

namespace MoonEngine.Install {
[ComImport, Guid("43826D1E-E718-42EE-BC55-A1E261C37BFE"),
 InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IShellItem {
    void BindToHandler(IntPtr context, ref Guid handler, ref Guid iid, out IntPtr result);
    void GetParent(out IShellItem parent);
    void GetDisplayName(uint kind, out IntPtr name);
    void GetAttributes(uint mask, out uint attributes);
    void Compare(IShellItem other, uint hint, out int order);
}

// Keep the complete native vtable order, including unused methods.
[ComImport, Guid("947AAB5F-0A5C-4C13-B4D6-4BF7836FC9F8"),
 InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IFileOperation {
    void Advise(IntPtr sink, out uint cookie);
    void Unadvise(uint cookie);
    void SetOperationFlags(uint flags);
    void SetProgressMessage([MarshalAs(UnmanagedType.LPWStr)] string message);
    void SetProgressDialog(IntPtr dialog);
    void SetProperties(IntPtr properties);
    void SetOwnerWindow(IntPtr window);
    void ApplyPropertiesToItem(IShellItem item);
    void ApplyPropertiesToItems(IntPtr items);
    void RenameItem(IShellItem item, [MarshalAs(UnmanagedType.LPWStr)] string name, IntPtr sink);
    void RenameItems(IntPtr items, [MarshalAs(UnmanagedType.LPWStr)] string name);
    void MoveItem(IShellItem item, IShellItem destination,
                  [MarshalAs(UnmanagedType.LPWStr)] string name, IntPtr sink);
    void MoveItems(IntPtr items, IShellItem destination);
    void CopyItem(IShellItem item, IShellItem destination,
                  [MarshalAs(UnmanagedType.LPWStr)] string name, IntPtr sink);
    void CopyItems(IntPtr items, IShellItem destination);
    void DeleteItem(IShellItem item, IntPtr sink);
    void DeleteItems(IntPtr items);
    void NewItem(IShellItem destination, uint attributes,
                 [MarshalAs(UnmanagedType.LPWStr)] string name,
                 [MarshalAs(UnmanagedType.LPWStr)] string template, IntPtr sink);
    void PerformOperations();
    void GetAnyOperationsAborted([MarshalAs(UnmanagedType.Bool)] out bool aborted);
}

public static class Transfer {
    [DllImport("shell32.dll", CharSet = CharSet.Unicode, PreserveSig = false)]
    static extern void
    SHCreateItemFromParsingName(string path, IntPtr context, ref Guid iid,
                                [MarshalAs(UnmanagedType.Interface)] out IShellItem item);

    static IShellItem Item(string path)
    {
        Guid iid = typeof(IShellItem).GUID;
        IShellItem item;
        SHCreateItemFromParsingName(path, IntPtr.Zero, ref iid, out item);
        return item;
    }

    static void Release(object value)
    {
        if (value != null && Marshal.IsComObject(value))
            Marshal.FinalReleaseComObject(value);
    }

    public static void Copy(string source, string destination, string name, bool quiet)
    {
        if (String.IsNullOrEmpty(name) || name != System.IO.Path.GetFileName(name) || name == "." ||
            name == "..")
            throw new ArgumentException("Invalid destination name");
        IShellItem from = null, to = null;
        IFileOperation operation = null;
        try {
            from = Item(source);
            to = Item(destination);
            operation = (IFileOperation)Activator.CreateInstance(
                Type.GetTypeFromCLSID(new Guid("3AD05575-8857-4850-9277-11B85BDB8E09")));
            // Confirm replacements through the Shell; never silently overwrite an ISO.
            operation.SetOperationFlags(quiet ? 0x614U : 0x200U);
            operation.CopyItem(from, to, name, IntPtr.Zero);
            operation.PerformOperations();
            bool aborted;
            operation.GetAnyOperationsAborted(out aborted);
            if (aborted)
                throw new OperationCanceledException("Transfer cancelled");
        } finally {
            Release(operation);
            Release(to);
            Release(from);
        }
    }
}
}
