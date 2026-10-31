/* DUDI 7 conformance harness, written in C on purpose: if plain C can drive
 * the plugin, the contract only uses plain types.
 * Usage: harness plugin.so fixtures_dir (see make_fixtures.py) */
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dudi7.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

static void *lib;
static void *sym(const char *n) { void *p = dlsym(lib, n); if (!p) { printf("missing export %s\n", n); exit(2); } return p; }

typedef struct { unsigned char *data; int64_t len, cap; } sink;
static int64_t on_write(void *ctx, const void *buf, int64_t len) {
    sink *s = ctx;
    if (s->len + len > s->cap) { s->cap = (s->len + len) * 2; s->data = realloc(s->data, s->cap); }
    memcpy(s->data + s->len, buf, len); s->len += len; return len;
}
static int64_t on_write_fail(void *ctx, const void *buf, int64_t len) { (void)ctx; (void)buf; (void)len; return 0; }

int main(int argc, char **argv) {
    if (argc < 3) return 2;
    lib = dlopen(argv[1], RTLD_NOW);
    if (!lib) { printf("dlopen: %s\n", dlerror()); return 2; }

    int32_t (*Version)(int32_t, int32_t *) = sym("Dudi7_Version");
    int32_t (*Init)(const dudi7_host *) = sym("Dudi7_Init");
    int32_t (*Shutdown)(void) = sym("Dudi7_Shutdown");
    int32_t (*DrvInfo)(dudi7_driver_info *) = sym("Dudi7_GetDriverInfo");
    int32_t (*FmtInfo)(int32_t, dudi7_format_info *) = sym("Dudi7_GetFormatInfo");
    int32_t (*IsFormat)(const char *, int32_t, int32_t *) = sym("Dudi7_IsFormat");
    int32_t (*Open)(const char *, int32_t, void **) = sym("Dudi7_Open");
    int32_t (*ArcInfo)(void *, dudi7_archive_info *) = sym("Dudi7_GetArchiveInfo");
    int32_t (*GetEntry)(void *, int64_t, dudi7_entry *) = sym("Dudi7_GetEntry");
    int32_t (*Extract)(void *, int64_t, dudi7_write_cb, void *) = sym("Dudi7_Extract");
    int32_t (*Close)(void *) = sym("Dudi7_Close");
    int32_t (*LastError)(void *, char *, int32_t) = sym("Dudi7_GetLastError");

    int32_t pv = 0;
    CHECK(Version(DUDI7_CONTRACT_VERSION, &pv) == DUDI7_OK && pv == DUDI7_CONTRACT_VERSION);
    CHECK(Version(DUDI7_CONTRACT_VERSION + 99, &pv) == DUDI7_ERR_VERSION);

    dudi7_host host = { sizeof host, DUDI7_CONTRACT_VERSION, NULL, NULL, NULL, NULL };
    CHECK(Init(&host) == DUDI7_OK);

    /* struct sizes as written by the (Pascal) plugin must match C's */
    dudi7_driver_info di; memset(&di, 0, sizeof di);
    CHECK(DrvInfo(&di) == DUDI7_OK);
    CHECK(di.struct_size == sizeof di);
    CHECK(di.format_count == 135 && di.name && !strcmp(di.name, "Default Driver"));
    dudi7_format_info fi; memset(&fi, 0, sizeof fi);
    CHECK(FmtInfo(0, &fi) == DUDI7_OK && fi.struct_size == sizeof fi);
    CHECK(fi.extensions && !strcmp(fi.extensions, "*.PAK"));
    for (int i = 0; i < di.format_count; i++) {
        memset(&fi, 0, sizeof fi);
        CHECK(FmtInfo(i, &fi) == DUDI7_OK && fi.extensions && fi.name);
    }
    CHECK(FmtInfo(di.format_count, &fi) == DUDI7_ERR_BADARG);
    CHECK(FmtInfo(-1, &fi) == DUDI7_ERR_BADARG);

    int32_t ok = -1;
    CHECK(IsFormat(argv[1], 0, &ok) == DUDI7_OK && ok == 0);   /* the .so is no archive */
    void *a = (void *)1;
    CHECK(Open(argv[1], 0, &a) == DUDI7_ERR_NOTFORMAT && a == NULL);
    char err[128]; CHECK(LastError(NULL, err, sizeof err) > 0);
    printf("expected error: %s\n", err);
    CHECK(Open("/nonexistent/file.pak", 0, &a) == DUDI7_ERR_IO);

    /* one fixture per format: same 3 payloads, names depend on the format */
    /* flags */
#define F_BLOCKS 1   /* sizes are rounded up to 2048 bytes blocks */
#define F_RIFF   2   /* data starts with RIFF instead of the payload */
#define F_ANYSIZE 4  /* the entries given as converted data: the size of the listing is only about the same */
    /* n entries; pay[i] = index of the payload (24 bytes, 76800 bytes, empty) of entry i, -1: only the name is checked */
    static const struct { const char *file, *id, *sep; int n; const char *names[16]; int pay[16]; int flags; } fx[] = {
        { "test.pak",  "PACK",  "/",  3, { "hello.txt", "sub/dir/data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "test.iwad", "IWAD",  "",   3, { "HELLO", "DATA", "EMPTY" }, { 0, 1, 2 } },
        { "test.pwad", "PWAD",  "",   3, { "HELLO", "DATA", "EMPTY" }, { 0, 1, 2 } },
        { "test.wad2", "WAD2",  "",   3, { "HELLO.pal", "DATA.mm", "EMPTY.pcp" }, { 0, 1, 2 } },
        { "test.gob",  "DGOB",  "",   3, { "HELLO.TXT", "DATA.TXT", "EMPTY.TXT" }, { 0, 1, 2 } },
        { "test.grp",  "KSGRP", "",   3, { "HELLO.DAT", "DATA.DAT", "EMPTY.DAT" }, { 0, 1, 2 } },
        { "test.spe",  "SPEC",  "/",  3, { "Image/HELLO.img", "Palette/DATA", "Normal file/EMPTY" }, { 0, 1, 2 } },
        { "test.dpa",  "DPA",   "/",  3, { "Palette/pal", "Modern item/hud.png", "Palette/pal2" }, { 0, -1, 0 } },
        { "test.rff",  "RFF",   "",   3, { "HELLO.TXT", "DATA.BIN", "EMPTY" }, { 0, 1, 2 } },
        { "test.art",  "ART",   "",   2, { "texture #000010.tex", "texture #000011.tex" }, { 0, 1 } },
        { "test.far",  "FAR",   "\\", 3, { "HELLO.DAT", "DATA.DAT", "EMPTY.DAT" }, { 0, 1, 2 } },
        { "test.vp",   "VP",    "\\", 2, { "data\\hello.txt", "data\\sub\\data.bin" }, { 0, 1 } },
        { "test.erf",  "ERF",   "/",  3, { "HELLO.dds", "DATA.dds", "EMPTY.dds" }, { 0, 1, 2 } },
        { "test.hog",  "HOG",   "",   3, { "HELLO.TXT", "DATA.BIN", "EMPTY.DAT" }, { 0, 1, 2 } },
        { "test.big",  "BIGF",  "/",  3, { "data/hello.txt", "data/sub/data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "test.drs",  "DRS",   "",   3, { "0000000001.txt", "0000000002.txt", "0000000003.wav" }, { 0, 1, 2 } },
        { "aoe3.bar",  "BAR",   "\\", 3, { "art\\hello.txt", "art\\data.bin", "art\\empty.dat" }, { 0, 1, 2 } },
        { "aom.bar",   "BAR",   "\\", 3, { "hello.txt", "data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "test.hal",  "HAL",   "",   3, { "hello.txt", "data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "test.bun",  "BUN",   "",   3, { "HELLO.TXT", "DATA.BIN", "EMPTY.DAT" }, { 0, 1, 2 } },
        { "test.bkf",  "BKF",   "\\", 3, { "HELLO.TXT", "sub\\DATA.BIN", "EMPTY.DAT" }, { 0, 1, 2 } },
        { "test.dat",  "DAT",   "",   2, { "hello", "data" }, { 0, 1 }, F_RIFF },
        { "test.syn",  "SYN",   "",   3, { "hello.txt", "data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "test.vol",  "PVOL",  "",   3, { "hello.txt", "data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "test.pod",  "POD",   "\\", 3, { "HELLO.TXT", "SUB\\DATA.BIN", "EMPTY.DAT" }, { 0, 1, 2 } },
        { "test.sin",  "SIN",   "/",  3, { "hello.txt", "sub/data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "count.sin", "SIN",   "/",  3, { "hello.txt", "sub/data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "test.snd",  "H3SND", "",   3, { "HELLO.wav", "DATA.bin", "EMPTY.txt" }, { 0, 1, 2 } },
        { "test.pff",  "PFF3",  "",   3, { "hello.txt", "data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "nascar.dat", "DAT",  "",   3, { "HELLO.TXT", "DATA.BIN", "EMPTY.DAT" }, { 0, 1, 2 } },
        { "sf.pak",    "MPF4",  "\\", 3, { "data\\hello.txt", "data\\data.bin", "other\\empty.dat" }, { 0, 1, 2 } },
        { "test.zfs",  "ZFS",   "",   3, { "hello.txt", "data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "test.sni",  "SNI",   "",   3, { "HELLO.WAV", "DATA.WAV", "EMPTY.WAV" }, { 0, 1, 2 } },
        { "test.pbo",  "PBO",   "\\", 3, { "data\\hello.txt", "data\\sub\\data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "bf.pak",    "BFPAK", "\\", 3, { "data\\hello.txt", "data\\data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "indy.gob",  "GOB",   "\\", 3, { "hello.txt", "sub\\data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "test.dwfb", "DWFB",  "\\", 3, { "data\\hello.txt", "data\\data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "pp.res",    "PPRES", "",   3, { "HELLO.TXT", "DATA.BIN", "EMPTY.DAT" }, { 0, 1, 2 } },
        { "test.awf",  "AWF",   "",   3, { "hello.txt", "data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "test.cub",  "CUB",   "\\", 3, { "data\\hello.txt", "data\\data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "test.cpr",  "CPR",   "\\", 3, { "data\\hello.txt", "data\\data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "test.fpk",  "FPK4",  "\\", 3, { "data\\hello.txt", "data\\data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "test.rez",  "REZ",   "\\", 3, { "data\\hello.txt", "data\\data.bin", "again.txt" }, { 0, 1, 0 } },
        { "test.box",  "BOX",   "\\", 3, { "data\\hello.txt", "data\\data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "test.cob",  "COB",   "\\", 3, { "hello.txt", "data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "ei.res",    "EIRES", "\\", 3, { "data\\hello.txt", "data\\data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "test.bnt",  "BNT2",  "\\", 3, { "data\\hello.txt", "data\\data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "test.pkr",  "PKR",   "/",  3, { "a/hello.txt", "a/data.bin", "b/empty.dat" }, { 0, 1, 2 } },
        { "pod2.pod",  "POD2",  "\\", 3, { "data\\hello.txt", "data\\data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "test.sqh",  "SQH",   "\\", 3, { "data\\a.txt", "data\\b.bin", "c.dat" }, { 0, 1, 2 } },
        { "fable.big", "BIGB",  "\\", 3, { "banks\\hello.txt", "banks\\data.bin", "banks\\empty.dat" }, { 0, 1, 2 } },
        { "fear.arch00", "LTAR", "\\", 3, { "sub\\hello.txt", "sub\\data.bin", "sub\\empty.dat" }, { 0, 1, 2 } },
        { "test.bag",  "BAG",   "",   3, { "hello.mp3", "data.w", "empty.raw" }, { 0, 1, 2 } },
        { "test.opk",  "OPK",   "\\", 3, { "data\\hello.txt", "data\\data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "test.oaf",  "OAF",   "\\", 3, { "data\\hello.txt", "data\\data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "sw.pak",    "Europa", "\\", 3, { "data\\hello.txt", "data\\data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "tm.pak",    "TMPAK", "\\", 3, { "data\\hello.txt", "data\\data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "test.ssa",  "SSA",   "\\", 3, { "data\\hello.txt", "data\\data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "test.dzip", "DZIP",  "\\", 3, { "data\\hello.txt", "data\\data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "pod3.pod",  "POD3",  "\\", 3, { "data\\hello.txt", "data\\data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "pod5.pod",  "POD5",  "\\", 3, { "data\\hello.txt", "data\\data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "test.gzp",  "GZP",   "",   3, { "hello.txt", "data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "test.rcf",  "RCF",   "\\", 3, { "hello.txt", "data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "test.mtf",  "MTF",   "\\", 3, { "data\\hello.txt", "data\\data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "retail.rfa", "RFA",  "/",  3, { "data/hello.txt", "data/data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "beta.rfa",  "RFA",   "/",  3, { "data/hello.txt", "data/data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "test.vfs",  "VFS",   "\\", 3, { "data\\hello.txt", "data\\data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "test.hpi",  "HPI",   "\\", 3, { "data\\hello.txt", "data\\data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "plain.ufo", "HPI",   "\\", 3, { "data\\hello.txt", "data\\data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "painkiller.pak", "PKPAK", "/", 3, { "data/hello.txt", "data/data.bin", "again.txt" }, { 0, 1, 0 } },
        { "dune.rfh",  "RFH/RFD", "\\", 3, { "data\\hello.txt", "data\\data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "test.xrs",  "PPXRS", "",   3, { "HELLO.TXT", "DATA.BIN", "EMPTY.DAT" }, { 0, 1, 2 } },
        { "test.stuff", "STUFF", "\\", 3, { "res:/hello.txt", "res:/data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "v1.tgp",    "TGP0",  "\\", 3, { "data\\hello.txt", "data\\data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "v2.tgp",    "TGP0",  "\\", 3, { "data\\hello.txt", "data\\data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "tonga.pak", "tonga", "\\", 3, { "data\\hello.txt", "data\\data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "v1.pck",    "GDPC",  "/",  3, { "data/hello.txt", "data/data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "v2.pck",    "GDPC",  "/",  3, { "data/hello.txt", "data/data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "v3.pck",    "GDPC",  "/",  3, { "data/hello.txt", "data/data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "cr5.pak",   "CRPAK", "",   3, { "101", "102", "103" }, { 0, 1, 2 } },
        { "cr4.pak",   "CRPAK", "",   3, { "101", "102", "103" }, { 0, 1, 2 } },
        { "ue3.pak",   "UEPAK3", "/", 3, { "Game/data/hello.txt", "Game/data/data.bin", "Game/empty.dat" }, { 0, 1, 2 } },
        { "ue7.pak",   "UEPAK7", "/", 3, { "Game/data/hello.txt", "Game/data/data.bin", "Game/empty.dat" }, { 0, 1, 2 } },
        { "ue8.pak",   "UEPAK8", "/", 3, { "Game/data/hello.txt", "Game/data/data.bin", "Game/empty.dat" }, { 0, 1, 2 } },
        { "ue9.pak",   "UEPAK9", "/", 3, { "Game/data/hello.txt", "Game/data/data.bin", "Game/empty.dat" }, { 0, 1, 2 } },
        { "ue11.pak",  "UEPAK11", "/", 3, { "Game/data/hello.txt", "Game/data/data.bin", "Game/empty.dat" }, { 0, 1, 2 } },
        { "test.sfl",  "SFL10", "\\", 3, { "data\\hello.txt", "data\\data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "bw2.stuff", "BW2STUFF", "\\", 3, { "data\\hello.txt", "data\\data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "nf1.007",   "007",   "\\", 3, { "empty.dat", "data\\hello.txt", "data\\data.bin" }, { 2, 0, 1 } },
        { "nf3.007",   "007",   "\\", 3, { "empty.dat", "data\\hello.txt", "data\\data.bin" }, { 2, 0, 1 } },
        { "aow.dat",   "EDAT",  "\\", 4, { "empty.dat", "data\\hello.txt", "data\\hello.txt", "data\\data.bin" }, { 2, 0, 0, 1 } },
        { "test.m4b",  "M4B",   "\\", 2, { "data\\hello.txt", "data\\data.bin" }, { 0, 1 } },
        { "ufs7.bundle", "UnityFS", "/", 3, { "CAB-test/hello.txt", "CAB-test/data.bin", "CAB-test.resS" }, { 0, 1, 2 } },
        { "ufs6.unity3d", "UnityFS", "/", 3, { "CAB-test/hello.txt", "CAB-test/data.bin", "CAB-test.resS" }, { 0, 1, 2 } },
        { "ufs8.bundle", "UnityFS", "/", 3, { "CAB-test/hello.txt", "CAB-test/data.bin", "CAB-test.resS" }, { 0, 1, 2 } },
        { "ufs8b.bundle", "UnityFS", "/", 3, { "CAB-test/hello.txt", "CAB-test/data.bin", "CAB-test.resS" }, { 0, 1, 2 } },
        { "test.assets", "UnityAssets", "/", 6, { "objects/Texture2D/rgba.tga", "objects/Texture2D/dxt5.dds", "objects/Texture2D/alpha.tga", "objects/TextAsset/cfg.json", "objects/AudioClip/snd.fsb", "objects/Font/fnt.otf" }, { -1, -1, -1, -1, -1, -1 } },
        { "test.wad", "DAWL", "\\", 3, { "XML\\30-00000001-00000000-00000000.xml", "Textures\\4\\4-00000002-00000000-00000000.tga", "99-00000003-00000000-00000000.063" }, { 0, 1, 2 } },
        { "test.dta", "DTA/CNT", "\\", 2, { "data\\hello.txt", "data\\data.bin" }, { 0, 1 } },
        { "test.cbf",  "CBF0", "\\", 3, { "hello.txt", "data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "crypt.cbf", "CBF1", "\\", 3, { "hello.txt", "data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "test.xcr",  "XCR",  "\\", 3, { "hello.txt", "game\\data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "test.sdt",  "SDT",  "", 3, { "hello.mp2", "data.mp2", "empty.mp2" }, { 0, 1, 2 } },
        { "test.gl",   "SCPAK", "", 3, { "HELLO", "DATA", "EMPTY" }, { 0, 1, 2 } },
        { "sc.pak",    "SCGL", "", 3, { "HELLO.wav", "DATA.wav", "EMPTY.wav" }, { 0, 1, 2 } },
        { "test.tex",  "GTEX", "\\", 3, { "tex\\hello.PNIT", "data.DXT1", "empty.RBGA" }, { -1, -1, -1 } },
        { "junk.zip",  "ZIP", "/", 3, { "dir/hello.txt", "raw.bin", "empty" }, { -1, -1, -1 } },
        { "test.dni",  "DNI",  "\\", 3, { "hello.txt", "sub\\data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "f22.dat",   "F22DAT", "", 3, { "01020304", "0A0B0C0D", "7F000001" }, { 0, 1, 2 } },
        { "lego.dat",  "LEGODAT", "\\", 3, { "data\\hello.txt", "data\\data.bin", "data\\empty.dat" }, { 0, 1, 2 } },
        { "test.jam",  "JAM2", "\\", 3, { "hello.txt", "sub\\data.bin", "sub\\empty.dat" }, { 0, 1, 2 } },
        { "test.gjd",  "GJD",  "", 3, { "HELLO.TXT", "DATA.BIN", "EMPTY.DAT" }, { 0, 1, 2 } },
        { "d3.hog",    "HOG2", "", 3, { "hello.txt", "data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "mr2.bkf",   "BKF2", "\\", 3, { "data\\hello.txt", "data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "test.h4r",  "H4R", "", 3, { "hello.txt", "data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "c3.pck",    "PCK", "\\", 4, { "hello.txt", "sub\\data.bin", "sub\\empty.dat", "secret.wav" }, { 0, 1, 2, -1 } },
        { "rom2.res",  "RoM2RES", "\\", 3, { "hello.txt", "sub\\data.bin", "sub\\empty.dat" }, { 0, 1, 2 } },
        { "nox.bag",   "BAG", "", 3, { "hello.mp3", "data.w", "empty.w" }, { 0, 1, 2 } },
        { "test.lug",  "LHAB", "\\", 2, { "Temp\\hello.txt", "Temp\\sub\\data.bin" }, { 0, 1 } },
        { "test.mrc",  "mmRC", "\\", 3, { "root\\hello.txt", "root\\sub\\data.bin", "root\\sub\\empty.dat" }, { 0, 1, 2 } },
        { "test.asr",  "Asura", "\\", 3, { "hello.txt", "sub\\data.bin", "snd\\empty.dat" }, { 0, 1, 2 } },
        { "testz.asr", "AsuraZ", "\\", 3, { "hello.txt", "sub\\data.bin", "snd\\empty.dat" }, { 0, 1, 2 } },
        { "cnc.mix",   "MIX", "\\", 3, { "00-i008.aud", "12345678", "9ABCDEF0" }, { 0, 1, 2 } },
        { "ww.pak",    "WWPAK", "", 3, { "empty.dat", "hello.txt", "data.bin" }, { 2, 0, 1 } },
        { "test.xwc",  "MOSD", "", 4, { "hello", "data", "empty", "song.ogg" }, { 0, 1, 2, -1 } },
        { "test.xtc",  "MOSDe", "", 1, { "tex.dds" }, { -1 } },
        { "ac.forge",  "FORGE", "", 3, { "hello.txt", "data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "heath.nrm", "NRM", "\\", 3, { "hello.txt", "data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "flor.pak",  "FLPAK", "", 3, { "hello.txt", "data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "test.utx",  "UTPKG", "\\", 4, { "Group\\hello.wav", "Group\\song.it", "Group\\pal.pal", "Group\\tex.bmp" }, { -1, -1, -1, -1 }, F_ANYSIZE },
        { "test.lfd", "DLFD", "", 3, { "beam.voic", "intro.text", "empty.pltt" }, { -1, -1, -1 } },
        { "test.xwb", "XWB", "", 3, { "wave_000.wav", "wave_001.wav", "wave_002.xma" }, { -1, -1, -1 } },
        { "test.win", "GMFORM", "/", 3, { "audio/audio_0.wav", "audio/snd_two_1.ogg", "textures/texture_0.png" }, { -1, -1, -1 } },
        { "test.zip", "ZIP", "/", 3, { "dir/hello.txt", "raw.bin", "empty" }, { -1, -1, -1 } },
        { "sfx.zip", "ZIP", "/", 3, { "dir/hello.txt", "raw.bin", "empty" }, { -1, -1, -1 } },
        { "test.fsb", "FSB5", "/", 3, { "boom.wav", "zap_x.wav", "boom_2.wav" }, { -1, -1, -1 } },
        { "mpeg.fsb", "FSB5", "/", 3, { "a.mp3", "b.mp3", "multi.mpeg.bin" }, { -1, -1, -1 } },
        { "vorbis.fsb", "FSB5", "/", 3, { "voice.ogg", "unknown.vorbis.bin", "voice2.ogg" }, { -1, -1, -1 } },
        { "vorbis.bank", "FSB5", "/", 3, { "voice.ogg", "unknown.vorbis.bin", "voice2.ogg" }, { -1, -1, -1 } },
        { "test.bank", "FSB5", "/", 3, { "boom.vorbis.bin", "zap_x.vorbis.bin", "boom_2.vorbis.bin" }, { -1, -1, -1 } },
        { "tex.xnb", "XNB", "/", 1, { "tex.tga" }, { -1 } },
        { "snd.xnb", "XNB", "/", 1, { "snd.wav" }, { -1 } },
        { "lzx.xnb", "XNB", "/", 1, { "lzx.tga" }, { -1 } },
        { "font.xnb", "XNB", "/", 2, { "font.tga", "font.fnt" }, { -1, -1 } },
        { "objs3.bundle", "UnityFS", "/", 13, { "CAB-o3", "CAB-o3.objects/Texture2D/atlas.tga", "CAB-o3.objects/Texture2D/blocks.dds", "CAB-o3.objects/Sprite/spr_a.tga",
          "CAB-o3.objects/Sprite/spr_b.tga", "CAB-o3.objects/Cubemap/cube_px.tga", "CAB-o3.objects/Cubemap/cube_nx.tga", "CAB-o3.objects/Cubemap/cube_py.tga",
          "CAB-o3.objects/Cubemap/cube_ny.tga", "CAB-o3.objects/Cubemap/cube_pz.tga", "CAB-o3.objects/Cubemap/cube_nz.tga", "CAB-o3.objects/Mesh/quad.obj",
          "CAB-o3.objects/Mesh/strip.obj" }, { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 } },
        { "ufs22.bundle", "UnityFS", "/", 4, { "CAB-obj", "CAB-obj.resS", "CAB-obj.objects/TextAsset/hello.txt", "CAB-obj.objects/Texture2D/data.tga" }, { -1, 1, 0, -1 } },
        { "zan.pak",   "ZPAK",  "\\", 3, { "data\\hello.txt", "data\\data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "kapl.pak",  "KAPL",  "/",  3, { "data/hello.txt", "data/data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "tlj_test.pak", "TLJPAK", "/", 2, { "tlj_test_pak-1.unk6C6C6548", "tlj_test_pak-3.unk03020100" }, { 0, 1 } },
        { "test.sak",  "SAK",   "/",  3, { "data/hello.txt", "data/data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "test.bsa",  "BSA",   "\\", 3, { "data\\hello.txt", "data\\data.bin", "sub\\empty.dat" }, { 0, 1, 2 } },
        { "test.slf",  "SLF",   "\\", 3, { "data\\hello.txt", "data\\data.bin", "data\\empty.dat" }, { 0, 1, 2 } },
        { "avp.rff",   "RFFL",  "\\", 3, { "hello.txt", "data.bin", "empty.dat" }, { 0, 1, 2 } },
        { "test.prm",  "HMCPRM", "",  3, { "test_prm_00000001.raw", "test_prm_00000002.raw", "test_prm_00000003.raw" }, { 0, 1, 2 } },
        { "cb.bin",    "CBIN",  "",   3, { "HELLO.TXT", "DATA.BIN", "EMPTY.DAT" }, { 0, 1, 2 } },
        { "gta3.img",  "DIR/IMG", "", 3, { "hello.txt", "data.bin", "empty.dat" }, { 0, 1, 2 }, F_BLOCKS },
        { "gta3.dir",  "DIR/IMG", "", 3, { "hello.txt", "data.bin", "empty.dat" }, { 0, 1, 2 }, F_BLOCKS },
        { "req_d3d.dat", "Requiem ART", "", 3, { "tex_0000.tga", "tex_0002.tex", "tex_0003.tga" }, { -1, -1, -1 } },
        { "req_d3d.art", "Requiem ART", "", 3, { "tex_0000.tga", "tex_0002.tex", "tex_0003.tga" }, { -1, -1, -1 } },
        { "req_sw.art", "Requiem ART", "", 2, { "tex_0000.tga", "tex_0002.tga" }, { -1, -1 } },
        { "req_static.mdl", "Requiem MDL", "", 2, { "req_static.obj", "req_static.mtl" }, { -1, -1 } },
        { "req_skel.mdl", "Requiem MDL (skeletal)", "", 2, { "req_skel.obj", "req_skel.mtl" }, { -1, -1 } },
        { "req_anim.ary", "Requiem ANIM", "", 3, { "anim_000.bin", "anim_001.bin", "anim_002.bin" }, { -1, -1, -1 } },
        { "req_anim.bin", "Requiem ANIM", "", 3, { "anim_000.bin", "anim_001.bin", "anim_002.bin" }, { -1, -1, -1 } },
        { "req_v0.jam", "Requiem JAM", "", 5, { "header.bin", "level.bin", "rest.bin", "data.bin", "palette.pal" }, { -1, -1, -1, -1, -1 } },
        { "req_v1.jam", "Requiem JAM", "", 3, { "header.bin", "level.bin", "rest.bin" }, { -1, -1, -1 } },
        { "req_v2.jam", "Requiem JAM", "", 5, { "header.bin", "level.bin", "rest.bin", "data.bin", "palette.pal" }, { -1, -1, -1, -1, -1 } },
    };
    static const int64_t sizes[3] = { 24, 256 * 300, 0 };
    sink s = {0};
    for (unsigned f = 0; f < sizeof fx / sizeof fx[0]; f++) {
        char path[512]; snprintf(path, sizeof path, "%s/%s", argv[2], fx[f].file);
        printf("-- %s\n", fx[f].file);
        ok = -1;
        CHECK(IsFormat(path, 0, &ok) == DUDI7_OK && ok == 1);
        a = NULL;
        CHECK(Open(path, 0, &a) == DUDI7_OK && a);
        if (!a) continue;
        dudi7_archive_info ai; memset(&ai, 0, sizeof ai);
        CHECK(ArcInfo(a, &ai) == DUDI7_OK && ai.struct_size == sizeof ai);
        CHECK(ai.entry_count == fx[f].n && !strcmp(ai.format_id, fx[f].id) && !strcmp(ai.dir_separator, fx[f].sep));
        dudi7_entry e; memset(&e, 0, sizeof e);
        for (int i = 0; i < fx[f].n; i++) {
            int p = fx[f].pay[i];
            CHECK(GetEntry(a, i, &e) == DUDI7_OK && e.struct_size == sizeof e);
            if (p < 0) {
                CHECK(!strcmp(e.name, fx[f].names[i]));
                s.len = 0;
                CHECK(Extract(a, i, on_write, &s) == DUDI7_OK && ((fx[f].flags & F_ANYSIZE) || s.len == e.size));
                continue;
            }
            int64_t want = sizes[p];
            if (fx[f].flags & F_BLOCKS) want = (want + 2047) / 2048 * 2048;
            CHECK(!strcmp(e.name, fx[f].names[i]) && e.size == want);
            s.len = 0;
            CHECK(Extract(a, i, on_write, &s) == DUDI7_OK && s.len == want);
            if (fx[f].flags & F_RIFF) {
                CHECK(s.len >= 4 && !memcmp(s.data, "RIFF", 4));      /* from the RIFF tag on */
            } else {
                if (p == 0) CHECK(!memcmp(s.data, "Hello, Dragon UnPACKer!\n", 24));
                if (p == 1) for (int k = 0; k < sizes[p]; k++) if (s.data[k] != (unsigned char)k) { CHECK(0); break; }
            }
        }
        CHECK(GetEntry(a, fx[f].n, &e) == DUDI7_ERR_BADARG);
        CHECK(GetEntry(a, -1, &e) == DUDI7_ERR_BADARG);
        CHECK(Extract(a, fx[f].n > 1 ? 1 : 0, on_write_fail, NULL) == DUDI7_ERR_CANCELLED);
        CHECK(Extract(a, 999, on_write, &s) == DUDI7_ERR_BADARG);
        CHECK(Extract(a, 0, NULL, NULL) == DUDI7_ERR_BADARG);
        CHECK(Close(a) == DUDI7_OK);
    }

    /* RNC: a single entry, unpacked on extraction (methods 1 and 2) */
    char path[512];
    snprintf(path, sizeof path, "%s/payload.bin", argv[2]);
    FILE *pf = fopen(path, "rb");
    static unsigned char payload[65536];
    size_t plen = pf ? fread(payload, 1, sizeof payload, pf) : 0;
    if (pf) fclose(pf);
    CHECK(plen > 20000 && plen < sizeof payload);
    for (int m = 1; m <= 2; m++) {
        char file[64], id[8], name[64];
        snprintf(file, sizeof file, "payload_m%d.rnc", m);
        snprintf(id, sizeof id, "RNC%d", m);
        snprintf(name, sizeof name, "payload_m%d.bin", m);
        printf("-- %s\n", file);
        snprintf(path, sizeof path, "%s/%s", argv[2], file);
        a = NULL;
        CHECK(Open(path, 0, &a) == DUDI7_OK && a);
        if (!a) continue;
        dudi7_archive_info ai; memset(&ai, 0, sizeof ai);
        CHECK(ArcInfo(a, &ai) == DUDI7_OK && ai.entry_count == 1 && ai.extract_internal == 1 && !strcmp(ai.format_id, id));
        dudi7_entry e; memset(&e, 0, sizeof e);
        CHECK(GetEntry(a, 0, &e) == DUDI7_OK && !strcmp(e.name, name) && e.size == (int64_t)plen);
        s.len = 0;
        CHECK(Extract(a, 0, on_write, &s) == DUDI7_OK && s.len == (int64_t)plen && !memcmp(s.data, payload, plen));
        s.len = 0;
        CHECK(Extract(a, 0, on_write_fail, NULL) == DUDI7_ERR_CANCELLED);
        CHECK(Close(a) == DUDI7_OK);
    }
    printf("-- corrupt.rnc\n");
    snprintf(path, sizeof path, "%s/corrupt.rnc", argv[2]);
    a = NULL;
    CHECK(Open(path, 0, &a) == DUDI7_OK && a);
    if (a) {
        CHECK(Extract(a, 0, on_write, &s) == DUDI7_ERR_IO);
        CHECK(LastError(a, err, sizeof err) > 0);
        printf("expected error: %s\n", err);
        CHECK(Close(a) == DUDI7_OK);
    }

    a = NULL;
    snprintf(path, sizeof path, "%s/test.pak", argv[2]);
    CHECK(Open(path, 0, &a) == DUDI7_OK);
    CHECK(Close(a) == DUDI7_OK);
    CHECK(Shutdown() == DUDI7_OK);
    free(s.data);
    dlclose(lib);
    printf(failures ? "%d FAILURE(S)\n" : "ALL OK\n", failures);
    return failures != 0;
}
