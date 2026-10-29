# DUDI 7: Dragon UnPACKer Driver Interface (plain C edition)

The contract between the host (the Qt interface) and the driver plugins. It is a C ABI: a plugin can be written in
any language that exports cdecl functions, and the host can be driven from C. The types and the exports are declared in
`dudi7.h` (the tests check its layout); this file keeps the rules and the order of the calls, which were documented in
the Pascal unit `spec_DUDI7.pas` (in the git history, with the rest of the Pascal sources).

## Rules

1. Every export and every callback is cdecl.
2. Only fixed-width integers, pointers and `char *` cross the boundary: no booleans, `ShortString`, streams, classes or
   dynamic arrays, and no record is passed or returned by value.
3. All strings are UTF-8, null terminated.
4. Every export returns an `int32_t` status (`DUDI7_OK` or a negative `DUDI7_ERR_*` code); results go through pointer
   parameters.
5. No exception leaves an export: plugins catch everything and map it to a status code, the message being available
   through `Dudi7_GetLastError`.
6. Memory never changes owner. Strings returned by the plugin belong to it and stay valid until the archive is closed
   (data of an archive) or until the next call to the same export (data of the driver). The host never frees plugin memory
   and the plugin never frees host memory.
7. Every shared record starts with `struct_size` (the size of the record as its writer knows it) so that it can grow.
8. Plugins open no user interface: whatever must be shown (about text, errors) is returned as text.
9. Files are opened by the plugin from the UTF-8 path that the host gives; no operating system handle is shared.

## Calls, in order

| Call | Use |
|---|---|
| `Dudi7_Version(host version)` | the plugin checks that it can work with the host |
| `Dudi7_Init(host callbacks)` | once, right after loading |
| `Dudi7_GetDriverInfo`, `Dudi7_GetFormatInfo` | what the driver can open |
| `Dudi7_IsFormat(path)` | cheap test: could this file be one of mine? |
| `Dudi7_Open(path)` gives a handle | parses the file |
| `Dudi7_GetArchiveInfo`, `Dudi7_GetEntry(handle, index)` | the entries |
| `Dudi7_Extract(handle, index, write callback)` | mandatory for every driver |
| `Dudi7_Close(handle)` | |
| `Dudi7_Shutdown` | |

## Status codes

`DUDI7_OK` 0, `DUDI7_ERR_GENERIC` -1 (see `GetLastError`), `DUDI7_ERR_VERSION` -2 (contract mismatch), `DUDI7_ERR_BADARG` -3
(null pointer, bad index), `DUDI7_ERR_NOTFORMAT` -4, `DUDI7_ERR_IO` -5, `DUDI7_ERR_UNSUPPORTED` -6, `DUDI7_ERR_CANCELLED` -7 (the
write callback asked to stop), `DUDI7_ERR_BUFFER` -8 (the caller's buffer is too small).

## Callbacks of the host

`progress(ctx, percent)`, `language(ctx, id, buf, buflen)` (length of the text, or negative if unknown), `message(ctx, title, msg)`
and the sink of `Extract`: `write(ctx, buf, len)` returns the number of bytes it took (`len` when all is well); anything else
stops the extraction. The fields `data_x` and `data_y` of an entry are free for the driver (they are not pointers); the host
hands them back to the converters.

`ExtractInternal` of the archive info: 1 when the data must be extracted through `Dudi7_Extract` (compressed, encrypted), 0 when
`offset` and `size` describe the raw data in the file.
