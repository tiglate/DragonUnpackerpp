# make.ps1 -- the Windows counterpart of make.sh: build, run, test, install and package Dragon UnPACKer++ with MSVC.
#
#   driver/   the format driver (C++20, a plain C ABI shared library, "DUDI 7"): build\plugins\drv_default.d6d
#   app/      the Qt 6 interface (C++20, CMake + Ninja): app\build\src\dragonunpacker.exe
#
# See README.md for the prerequisites (Visual Studio C++ tools, Qt 6, vcpkg packages, WiX) and CLAUDE.md for the architecture.
#
#   .\make.ps1 <action> [target] [options] [-- application arguments]
#
# License: MPL 2.0, as the rest of the project.

[CmdletBinding(PositionalBinding = $false)]
param(
    [Parameter(Position = 0)]
    [string]$Action = "",

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Rest = @()
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$SourceDir = $PSScriptRoot
$DriverDir = Join-Path $SourceDir "driver"
$AppDir = Join-Path $SourceDir "app"
$BuildDir = Join-Path $SourceDir "build"
$AppBuildDir = Join-Path $AppDir "build"
$DistDir = Join-Path $SourceDir "dist"
$InstallPrefix = Join-Path $SourceDir "install"
$Plugin = Join-Path $BuildDir "plugins\drv_default.d6d"
$Cli = Join-Path $BuildDir "tools\dudi7cli.exe"
$AppExe = Join-Path $AppBuildDir "src\dragonunpacker.exe"
$Jobs = [Environment]::ProcessorCount
$Verbose = $false
$Rebuild = $false
$Triplet = "x64-windows"

# Python and the Qt tests print UTF-8
$env:PYTHONUTF8 = "1"

function Show-Usage {
    @"
Usage: .\make.ps1 <action> [target] [options] [-- application arguments]

Actions:
  build [target]   Compile. Targets: "all" (default: driver, then the interface), "driver" (the plugin
                    drv_default.d6d and the dudi7cli test tool), "ui" (the Qt interface), "cli" (the
                    same build as "driver").
  rebuild [target] Same as "build" but recompiles everything.
  run              Build, then start the interface. Everything after "--" is passed to it:
                    --open=FILE --folder=a/b --select=NAME --theme=system|dark|light --recursive --sort=COL
                    --extract-all=DIR | --extract-selected=DIR --screenshot=F.png --quit
                    (add -platform offscreen when there is no display).
  test             Build, then run every automated test that needs no game files: the driver and the interface.
  test-driver      Only the driver tests (C harness, fuzzing, decoders against reference implementations).
  test-ui          Only the interface tests (unit tests and smoke tests that start the application offscreen).
  e2e FILE...      End to end: the interface extracts each archive (offscreen) and the MD5 of every
                    file is compared with the driver listing.
  sweep DIR...     False-positive sweep of the driver on the files under DIR.
  bench FILE       Time the opening and indexing of an archive (no window).
  list FILE        List an archive with the command line tool (name, offset, size, MD5).
  extract FILE DIR Extract every entry of an archive into DIR with the command line tool.
  configure        Check the prerequisites and show what was found.
  clean            Remove build outputs.
  purge            Same as clean, plus install\, dist\ and Python caches.
  declutter        Remove the Python caches.
  install          Build and install under the prefix (dragonunpacker.exe, Qt DLLs, plugins\drv_default.d6d).
  package          Build and make a zip of the installation (dist\).
  msi              Build and make an .msi installer (dist\) with the WiX Toolset (v4 or later, "wix" on PATH).

Options:
  -p, --prefix DIR   Install prefix (default: .\install).
  -j, --jobs N       Parallel compiler processes (default: all cores, currently $Jobs).
  -B, --rebuild-all  Recompile everything.
  -v, --verbose      Show the compiler output instead of the last lines.
  -h, --help         Show this help.

Environment (all optional, found by themselves when installed in the usual places):
  QT_ROOT            Qt 6 for MSVC, for example C:\Qt\6.8.3\msvc2022_64 (also QTDIR, Qt6_DIR or CMAKE_PREFIX_PATH).
  VCPKG_ROOT         vcpkg checkout with spdlog, ms-gsl and zlib installed for $Triplet.
  DUP_PLUGIN         Driver used by "run" and the tests in place of build\plugins\drv_default.d6d.

Examples:
  .\make.ps1 build
  .\make.ps1 run -- --open=C:\games\Quake\id1\pak0.pak
  .\make.ps1 test
  .\make.ps1 list C:\games\Quake\id1\pak0.pak
  .\make.ps1 msi
"@
}

function Fail([string]$Message) {
    Write-Host "make.ps1: error: $Message" -ForegroundColor Red
    exit 1
}

function Find-Program([string]$Name) {
    $found = Get-Command $Name -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) { return $found.Source }
    return $null
}

function Require-Program([string]$Name) {
    if (-not (Find-Program $Name)) { Fail "'$Name' not found on PATH (see README.md, section Building)" }
}

# ---- environment: Visual Studio (cl, cmake, ninja), Qt, vcpkg ---------------------------------------------------------

$script:EnvReady = $false
# vcvars64.bat replaces VCPKG_ROOT with the vcpkg bundled with Visual Studio: remember the caller's one first
$script:CallerVcpkgRoot = $env:VCPKG_ROOT
$script:QtRoot = $null
$script:VcpkgRoot = $null

function Find-VisualStudio {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { return $null }
    $path = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -ne 0 -or -not $path) { return $null }
    return ($path | Select-Object -First 1)
}

function Find-QtRoot {
    $candidates = @($env:QT_ROOT, $env:QTDIR, $env:Qt6_DIR, $env:QT_ROOT_DIR) | Where-Object { $_ }
    foreach ($c in $candidates) {
        $c = $c -replace "[\\/]lib[\\/]cmake[\\/]Qt6$", ""
        if (Test-Path (Join-Path $c "lib\cmake\Qt6")) { return $c }
    }
    foreach ($base in @("C:\Qt", "C:\dev\Qt", (Join-Path $env:USERPROFILE "Qt"))) {
        if (-not (Test-Path $base)) { continue }
        $hit = Get-ChildItem $base -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '^6\.\d+' } |
            Sort-Object { [version]($_.Name -replace '[^\d.]', '') } -Descending |
            ForEach-Object { Get-ChildItem $_.FullName -Directory -Filter "msvc*_64" -ErrorAction SilentlyContinue } |
            Where-Object { Test-Path (Join-Path $_.FullName "lib\cmake\Qt6") } |
            Select-Object -First 1
        if ($hit) { return $hit.FullName }
    }
    return $null
}

function Find-VcpkgRoot {
    # the vcpkg of Visual Studio (VCPKG_ROOT after vcvars64.bat) has no packages: prefer the checkout that has spdlog
    $valid = @()
    foreach ($c in @($env:DUP_VCPKG_ROOT, $script:CallerVcpkgRoot, $env:VCPKG_ROOT, "C:\dev\vcpkg", "C:\vcpkg", (Join-Path $env:USERPROFILE "vcpkg"))) {
        if ($c -and (Test-Path (Join-Path $c "scripts\buildsystems\vcpkg.cmake"))) { $valid += $c }
    }
    foreach ($c in $valid) {
        if (Test-Path (Join-Path $c "installed\$Triplet\share\spdlog")) { return $c }
    }
    return ($valid | Select-Object -First 1)
}

# Loads the environment of "x64 Native Tools" into this process (cl, link, rc, cmake and ninja of Visual Studio), once.
function Initialize-Environment {
    if ($script:EnvReady) { return }
    if (-not (Find-Program "cl.exe")) {
        $vs = Find-VisualStudio
        if (-not $vs) { Fail "Visual Studio with the C++ tools (MSVC) was not found" }
        $vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"
        if (-not (Test-Path $vcvars)) { Fail "$vcvars not found" }
        $dump = & cmd.exe /d /c "`"$vcvars`" >nul 2>&1 && set"
        foreach ($line in $dump) {
            if ($line -match '^([^=]+)=(.*)$') { Set-Item -Path "Env:$($Matches[1])" -Value $Matches[2] }
        }
        if (-not (Find-Program "cl.exe")) { Fail "cl.exe is still not on PATH after vcvars64.bat" }
    }
    $script:QtRoot = Find-QtRoot
    $script:VcpkgRoot = Find-VcpkgRoot
    if ($script:QtRoot) {
        $env:PATH = (Join-Path $script:QtRoot "bin") + ";" + $env:PATH
    }
    if ($script:VcpkgRoot) {
        $env:VCPKG_ROOT = $script:VcpkgRoot # vcvars64.bat points it at the copy bundled with Visual Studio
        $env:PATH = (Join-Path $script:VcpkgRoot "installed\$Triplet\bin") + ";" + $env:PATH
    }
    $script:EnvReady = $true
}

# The tools are quiet when they work; show everything with -v, otherwise only on failure.
function Invoke-Quiet([string]$Exe, [string[]]$Arguments) {
    if ($Verbose) {
        & $Exe @Arguments
        if ($LASTEXITCODE -ne 0) { Fail "command failed ($LASTEXITCODE): $Exe $($Arguments -join ' ')" }
        return
    }
    $previous = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $log = & $Exe @Arguments 2>&1 | ForEach-Object { "$_" }
    $code = $LASTEXITCODE
    $ErrorActionPreference = $previous
    if ($code -ne 0) {
        $log | ForEach-Object { [Console]::Error.WriteLine($_) }
        Fail "command failed ($code): $Exe $($Arguments -join ' ')"
    }
    $log | Where-Object { $_ -match "error|fatal|Linking|warning C" } | Select-Object -Last 4 | ForEach-Object { Write-Host $_ }
}

function Invoke-Checked([string]$Exe, [string[]]$Arguments) {
    & $Exe @Arguments
    if ($LASTEXITCODE -ne 0) { Fail "command failed ($LASTEXITCODE): $Exe $($Arguments -join ' ')" }
}

function Get-PythonExe {
    foreach ($n in @("python", "python3", "py")) {
        $p = Find-Program $n
        # skip the Microsoft Store stub, which prints a message and exits
        if ($p -and ($p -notmatch "WindowsApps" -or (& $p -c "print(1)" 2>$null) -eq "1")) { return $p }
    }
    Fail "Python 3 not found on PATH (needed by the tests)"
}

# ---- actions ------------------------------------------------------------------------------------------------------------

function Do-Configure {
    Initialize-Environment
    $cl = Find-Program "cl.exe"
    Write-Host "MSVC:    $cl"
    Write-Host "CMake:   $(Find-Program 'cmake.exe')  Ninja: $(Find-Program 'ninja.exe')"
    if ($script:QtRoot) { Write-Host "Qt:      $script:QtRoot" } else { Write-Host "Qt:      NOT FOUND (set QT_ROOT; needed for the interface)" -ForegroundColor Yellow }
    if ($script:VcpkgRoot) {
        Write-Host "vcpkg:   $script:VcpkgRoot"
        foreach ($pkg in "zlib", "spdlog", "ms-gsl") {
            $info = Join-Path $script:VcpkgRoot "installed\vcpkg\info"
            $has = (Get-ChildItem $info -Filter "${pkg}_*_$Triplet.list" -ErrorAction SilentlyContinue | Measure-Object).Count -gt 0
            if (-not $has) { Write-Host "         package ${pkg}:${Triplet} is NOT installed (vcpkg install ${pkg}:${Triplet})" -ForegroundColor Yellow }
        }
    } else { Write-Host "vcpkg:   NOT FOUND (set VCPKG_ROOT; zlib, spdlog and ms-gsl come from it)" -ForegroundColor Yellow }
    try { Write-Host "Python:  $(Get-PythonExe)" } catch { Write-Host "Python:  not found (the tests need it)" -ForegroundColor Yellow }
    $wix = Find-Program "wix.exe"
    if ($wix) { Write-Host "WiX:     $wix ($(& $wix --version))" } else { Write-Host "WiX:     not found ('dotnet tool install --global wix'; only the msi action needs it)" -ForegroundColor Yellow }
}

function Get-CMakeCommonArguments {
    $args = @("-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release")
    if ($script:VcpkgRoot) {
        $args += "-DCMAKE_TOOLCHAIN_FILE=$($script:VcpkgRoot -replace '\\', '/')/scripts/buildsystems/vcpkg.cmake"
        $args += "-DVCPKG_TARGET_TRIPLET=$Triplet"
    }
    return $args
}

function Build-Driver {
    Initialize-Environment
    Require-Program "cmake.exe"
    Require-Program "ninja.exe"
    $a = (Get-CMakeCommonArguments) + @("-S", "$DriverDir\cpp", "-B", "$BuildDir\cpp")
    Invoke-Quiet "cmake.exe" $a
    $b = @("--build", "$BuildDir\cpp", "--parallel", "$Jobs")
    if ($Rebuild) { $b += "--clean-first" }
    Invoke-Quiet "cmake.exe" $b
}

function Build-Cli { Build-Driver }

function Build-Ui {
    Initialize-Environment
    Require-Program "cmake.exe"
    Require-Program "ninja.exe"
    if (-not $script:QtRoot) { Fail "Qt 6 for MSVC not found: set QT_ROOT (for example C:\Qt\6.8.3\msvc2022_64)" }
    if (-not (Test-Path "$AppBuildDir\build.ninja") -or $script:Reconfigure) {
        $a = (Get-CMakeCommonArguments) + @("-S", $AppDir, "-B", $AppBuildDir, "-DCMAKE_PREFIX_PATH=$($script:QtRoot -replace '\\', '/')")
        Invoke-Quiet "cmake.exe" $a
    }
    $b = @("--build", $AppBuildDir, "--parallel", "$Jobs")
    if ($Rebuild) { $b += "--clean-first" }
    Invoke-Quiet "cmake.exe" $b
}

function Do-Build([string]$Target) {
    switch ($(if ($Target) { $Target } else { "all" })) {
        "all" { Build-Driver; Build-Ui }
        "driver" { Build-Driver }
        "ui" { Build-Ui }
        "cli" { Build-Cli }
        default { Fail "unknown build target: $Target (all, driver, ui, cli)" }
    }
    Write-Host "built: $(if ($Target) { $Target } else { 'all' })"
}

function Get-PluginPath { if ($env:DUP_PLUGIN) { return $env:DUP_PLUGIN } else { return $Plugin } }

function Do-Run([string[]]$AppArguments) {
    Do-Build "all"
    Write-Host "starting the interface"
    $env:DUP_PLUGIN = Get-PluginPath
    & $AppExe @AppArguments
    exit $LASTEXITCODE
}

function New-TempDirectory {
    $d = Join-Path ([IO.Path]::GetTempPath()) ("dup-" + [IO.Path]::GetRandomFileName())
    New-Item -ItemType Directory -Force $d | Out-Null
    return $d
}

# Runs "python script args"; a failure stops the tests.
function Invoke-Python([string]$Script, [string[]]$Arguments) {
    $py = Get-PythonExe
    $out = & $py (Join-Path $DriverDir "tests\$Script") @Arguments
    if ($LASTEXITCODE -ne 0) { $out | ForEach-Object { Write-Host $_ }; Fail "$Script failed ($LASTEXITCODE)" }
    return $out
}

# "cli plugin file > listing" through cmd, which keeps the bytes (Set-Content would add a BOM and CRLF)
function Save-Listing([string]$Plugin, [string]$File, [string]$Listing) {
    & cmd.exe /d /c "`"$Cli`" `"$Plugin`" `"$File`" > `"$Listing`""
    if ($LASTEXITCODE -ne 0) { Fail "dudi7cli failed on $File" }
}

function Do-TestDriver {
    Build-Driver
    Initialize-Environment
    $plugin = if ($env:DUDI7_PLUGIN) { $env:DUDI7_PLUGIN } else { Get-PluginPath }
    $tests = Join-Path $DriverDir "tests"
    $tmp = New-TempDirectory
    try {
        $fx = Join-Path $tmp "fx"
        $o = Join-Path $tmp "o"
        New-Item -ItemType Directory -Force $o | Out-Null

        # the C harness (proves the ABI is plain C), built with cl
        Invoke-Python "make_fixtures.py" @($fx) | Out-Null
        $harness = Join-Path $tmp "harness.exe"
        Invoke-Quiet "cl.exe" @("/nologo", "/W3", "/utf-8", "/D_CRT_SECURE_NO_WARNINGS", "/I$DriverDir\contract", "/Fe:$harness", "/Fo:$tmp\harness.obj", "$tests\harness.c")
        $harnessOut = & $harness $plugin $fx
        if ($LASTEXITCODE -ne 0) { $harnessOut | ForEach-Object { Write-Host $_ }; Fail "the C harness failed" }
        $harnessOut | Select-Object -Last 2 | ForEach-Object { Write-Host $_ }

        Invoke-Python "fuzz.py" @($plugin, $Cli, $fx, "40") | Select-Object -Last 3 | ForEach-Object { Write-Host $_ }
        Invoke-Python "verify_roms.py" @($plugin, $Cli) | Select-Object -Last 1 | ForEach-Object { Write-Host $_ }
        Invoke-Python "verify_requiem.py" @($plugin, $Cli, $fx) | ForEach-Object { Write-Host $_ }
        Invoke-Python "verify_lzo.py" @($plugin, $Cli) | Select-Object -Last 3 | ForEach-Object { Write-Host $_ }
        Invoke-Python "verify_lzma.py" @($plugin, $Cli) | Select-Object -Last 3 | ForEach-Object { Write-Host $_ }

        # listing and MD5 of every entry vs the independent python readers
        $groups = [ordered]@{
            "unity objects" = "obj17.bundle", "obj19.bundle", "obj21.bundle", "obj22.bundle", "obj22c.bundle", "ufs22.bundle", "notree0.bundle", "notree1.bundle", "notree2.bundle", "notree3.bundle", "texfmts.bundle", "objs3.bundle", "objs3_notree.bundle"
            "fmod" = "test.fsb", "mpeg.fsb", "test.bank", "vorbis.fsb", "vorbis.bank", "empty.bank"
            "xnb" = "tex.xnb", "dxt.xnb", "rgb565.xnb", "snd.xnb", "custom.xnb", "lz4.xnb", "font.xnb"
        }
        foreach ($group in $groups.Keys) {
            foreach ($name in $groups[$group]) {
                $listing = Join-Path $o "$name.lst"
                Save-Listing $plugin (Join-Path $fx $name) $listing
                $result = Invoke-Python "verify_real.py" @((Join-Path $fx $name), $listing)
                $result | ForEach-Object { Write-Host "${group}: $_" }
            }
        }
        foreach ($f in "lzx", "lzx_small", "font_lzx") {
            $a = (& $Cli $plugin (Join-Path $fx "$f.xnb") | Select-Object -First 1) -split "`t"
            $b = (& $Cli $plugin (Join-Path $fx "${f}_ref.xnb") | Select-Object -First 1) -split "`t"
            if (($a[2..3] -join "`t") -ne ($b[2..3] -join "`t")) { Fail "xnb: $f.xnb (LZX): data differs from the uncompressed file" }
            Write-Host "xnb: $f.xnb (LZX): same data as the uncompressed file"
        }

        # the converted textures keep the MD5s that texture2ddecoder agreed with
        $got = & $Cli $plugin (Join-Path $fx "texfmts.bundle") | Where-Object { $_ -notmatch '^#' -and $_ -ne "" } |
            ForEach-Object { $c = $_ -split "`t"; "$($c[3])`t$($c[4])" }
        $expected = Get-Content (Join-Path $tests "texfmts.expected") | Where-Object { $_ -ne "" }
        if (($got -join "`n") -ne ($expected -join "`n")) { Fail "textures: converted data differs from driver/tests/texfmts.expected" }
        Write-Host "textures: converted data unchanged"

        # decoders against texture2ddecoder (only when it is installed)
        $py = Get-PythonExe
        $previous = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        & $py -c "import texture2ddecoder, PIL" 2>&1 | Out-Null
        $hasDecoder = ($LASTEXITCODE -eq 0)
        $ErrorActionPreference = $previous
        if ($hasDecoder) {
            New-Item -ItemType Directory -Force (Join-Path $tmp "obj") | Out-Null
            $cpp = Join-Path $DriverDir "cpp\src"
            $cl = @("/nologo", "/std:c++20", "/O1", "/EHsc", "/utf-8", "/D_CRT_SECURE_NO_WARNINGS", "/DNOMINMAX", "/I$cpp")
            Invoke-Quiet "cl.exe" ($cl + @("/Fe:$tmp\texdec_dump.exe", "/Fo:$tmp\obj\", "$tests\texdec_dump.cpp", "$cpp\codecs\TexDecode.cpp", "$cpp\codecs\Astc.cpp"))
            Invoke-Quiet "cl.exe" ($cl + @("/Fe:$tmp\crn_dump.exe", "/Fo:$tmp\obj\", "$tests\crn_dump.cpp", "$cpp\codecs\Crunch.cpp"))
            Invoke-Python "verify_texdecode.py" @("$tmp\texdec_dump.exe", "$tmp\crn_dump.exe") | Select-Object -Last 2 | ForEach-Object { Write-Host $_ }
        } else {
            Write-Host "texture decoders: skipped (pip install texture2ddecoder pillow)"
        }

        # bundles with no file must open with 0 entries (unsigned loop counters)
        foreach ($f in "empty8.bundle", "empty6.unity3d") {
            $listing = & $Cli $plugin (Join-Path $fx $f) list 2>&1 | ForEach-Object { "$_" }
            if ($LASTEXITCODE -ne 0 -or -not ($listing -contains "#UnityFS`t0")) { Fail "empty bundle ${f}: not opened with 0 entries" }
            Write-Host "empty bundle ${f}: OK"
        }
        Invoke-Python "zero_entries.py" @($plugin, $Cli, $fx) | Select-Object -Last 3 | ForEach-Object { Write-Host $_ }
    } finally {
        Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
    }
    Write-Host "driver tests passed"
}

function Do-TestUi {
    Build-Driver
    Build-Ui
    $env:QT_QPA_PLATFORM = "offscreen"
    Invoke-Checked "ctest.exe" @("--test-dir", $AppBuildDir, "--output-on-failure")
}

function Do-Test {
    Do-TestDriver
    Do-TestUi
}

function Do-E2e([string[]]$Files) {
    if (-not $Files) { Fail "e2e needs at least one archive" }
    Build-Driver
    Build-Ui
    $py = Get-PythonExe
    $plugin = Get-PluginPath
    $tmp = New-TempDirectory
    $rc = 0
    try {
        foreach ($f in $Files) {
            $name = Split-Path $f -Leaf
            $in = Join-Path $tmp "in"; $dest = Join-Path $tmp "dest"
            New-Item -ItemType Directory -Force $in, $dest | Out-Null
            Copy-Item $f (Join-Path $in $name)
            Save-Listing $plugin (Join-Path $in $name) (Join-Path $tmp "listing")
            $env:DUP_PLUGIN = $plugin
            & $AppExe "--settings=$tmp\settings.ini" "--open=$in\$name" "--extract-all=$dest" --quit -platform offscreen *> (Join-Path $tmp "log")
            if ($LASTEXITCODE -ne 0) { Write-Host "${name}: the application failed"; Get-Content (Join-Path $tmp "log") -Tail 5; $rc = 1 }
            & $py "$DriverDir\tests\compare_extract.py" (Join-Path $tmp "listing") $dest $name
            if ($LASTEXITCODE -ne 0) { $rc = 1 }
            Remove-Item -Recurse -Force $in, $dest
        }
    } finally {
        Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
    }
    exit $rc
}

function Do-Sweep([string[]]$Dirs) {
    if (-not $Dirs) { Fail "sweep needs at least one directory" }
    Build-Driver
    Invoke-Checked (Get-PythonExe) (@("-I", "$DriverDir\tests\sweep.py", (Get-PluginPath), $Cli) + $Dirs)
}

function Do-Bench([string[]]$Files) {
    if ($Files.Count -ne 1) { Fail "usage: .\make.ps1 bench FILE" }
    Build-Driver
    Build-Ui
    $env:DUP_PLUGIN = Get-PluginPath
    Invoke-Checked $AppExe @("--benchmark=$($Files[0])")
}

function Do-List([string[]]$Files) {
    if ($Files.Count -ne 1) { Fail "usage: .\make.ps1 list FILE" }
    Build-Driver
    Invoke-Checked $Cli @((Get-PluginPath), $Files[0])
}

function Do-Extract([string[]]$Items) {
    if ($Items.Count -ne 2) { Fail "usage: .\make.ps1 extract FILE DIR" }
    Build-Driver
    New-Item -ItemType Directory -Force $Items[1] | Out-Null
    Invoke-Checked $Cli @((Get-PluginPath), $Items[0], "x", $Items[1])
}

function Do-Clean {
    Remove-Item -Recurse -Force $BuildDir, $AppBuildDir -ErrorAction SilentlyContinue
    Write-Host "removed the build outputs"
}

function Do-Declutter {
    Get-ChildItem "$DriverDir\tests" -Recurse -Directory -Filter "__pycache__" | Remove-Item -Recurse -Force
    Write-Host "removed the Python caches"
}

function Do-Purge {
    Do-Clean
    Remove-Item -Recurse -Force $InstallPrefix, $DistDir -ErrorAction SilentlyContinue
    Do-Declutter
    Write-Host "removed install\ and dist\"
}

function Do-Install {
    Build-Driver
    # configure again so that the install rules see the driver that was just built
    $script:Reconfigure = $true
    Build-Ui
    $script:Reconfigure = $false
    Invoke-Checked "cmake.exe" @("--install", $AppBuildDir, "--prefix", $InstallPrefix)
    # Qt's DLLs, platform and image plugins, translations of Qt itself
    $windeployqt = Join-Path $script:QtRoot "bin\windeployqt.exe"
    if (-not (Test-Path $windeployqt)) { Fail "$windeployqt not found" }
    Invoke-Quiet $windeployqt @("--release", "--no-translations", "--no-system-d3d-compiler", "--no-opengl-sw", "--no-compiler-runtime", (Join-Path $InstallPrefix "dragonunpacker.exe"))
    # the C++ runtime next to the executable (app-local), so that the installation needs no Visual C++ Redistributable
    $vsRoot = Find-VisualStudio
    $crt = Get-ChildItem (Join-Path $vsRoot "VC\Redist\MSVC") -Recurse -Directory -Filter "Microsoft.VC*.CRT" -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -like '*\x64\*' -and $_.FullName -notmatch 'onecore|debug' } | Sort-Object FullName | Select-Object -Last 1
    if (-not $crt) { Fail "the Visual C++ runtime DLLs were not found under $vsRoot\VC\Redist" }
    Copy-Item (Join-Path $crt.FullName "*.dll") $InstallPrefix -Force
    # the DLLs of the vcpkg packages (spdlog, fmt, zlib) that the build copied next to the executable
    Get-ChildItem "$AppBuildDir\src" -Filter "*.dll" | ForEach-Object { Copy-Item $_.FullName $InstallPrefix -Force }
    Copy-Item (Join-Path $SourceDir "LICENSE") (Join-Path $InstallPrefix "LICENSE.txt") -Force
    Write-Host "installed in $InstallPrefix (run $InstallPrefix\dragonunpacker.exe)"
}

function Get-ProjectVersion {
    $m = Select-String -Path (Join-Path $AppDir "CMakeLists.txt") -Pattern 'VERSION\s+(\d+\.\d+\.\d+)' | Select-Object -First 1
    if (-not $m) { Fail "cannot read the version from app/CMakeLists.txt" }
    return $m.Matches[0].Groups[1].Value
}

function Do-Package {
    $saved = $InstallPrefix
    $stage = Join-Path (New-TempDirectory) "dragonunpacker"
    $script:InstallPrefix = $stage
    try {
        Do-Install
        New-Item -ItemType Directory -Force $DistDir | Out-Null
        $out = Join-Path $DistDir "dragonunpacker-windows-x64.zip"
        Remove-Item $out -ErrorAction SilentlyContinue
        Compress-Archive -Path $stage -DestinationPath $out
        Write-Host "package: $out"
    } finally {
        $script:InstallPrefix = $saved
        Remove-Item -Recurse -Force (Split-Path $stage -Parent) -ErrorAction SilentlyContinue
    }
}

function Do-Msi {
    $wix = Find-Program "wix.exe"
    if (-not $wix) {
        $tool = Join-Path $env:USERPROFILE ".dotnet\tools\wix.exe"
        if (Test-Path $tool) { $wix = $tool }
    }
    if (-not $wix) { Fail "WiX Toolset not found: install it with 'dotnet tool install --global wix' (v4 or later)" }
    $saved = $InstallPrefix
    $stage = Join-Path (New-TempDirectory) "dragonunpacker"
    $script:InstallPrefix = $stage
    try {
        Do-Install
        $version = Get-ProjectVersion
        $wixDir = Join-Path $SourceDir "installer"
        # the license page of the installer wants RTF: wrap the plain text of LICENSE
        $rtf = Join-Path (Split-Path $stage -Parent) "license.rtf"
        $text = (Get-Content (Join-Path $SourceDir "LICENSE") -Raw) -replace '\\', '\\' -replace '\{', '\{' -replace '\}', '\}'
        $text = ($text -replace "`r?`n", "\par`r`n")
        $text = [regex]::Replace($text, '[^\x00-\x7f]', { param($m) "\u$([int][char]$m.Value)?" })
        [IO.File]::WriteAllText($rtf, "{\rtf1\ansi\deff0{\fonttbl{\f0 Consolas;}}\f0\fs18 $text}", [Text.Encoding]::ASCII)
        New-Item -ItemType Directory -Force $DistDir | Out-Null
        $out = Join-Path $DistDir "dragonunpacker-$version-x64.msi"
        Remove-Item $out -ErrorAction SilentlyContinue
        # the UI extension (WixUI_InstallDir) is a NuGet package: added once to the user's WiX extension cache
        $ext = & $wix extension list -g 2>$null | Out-String
        if ($ext -notmatch "WixToolset.UI.wixext") {
            Invoke-Checked $wix @("extension", "add", "-g", "WixToolset.UI.wixext/$(& $wix --version | Select-Object -First 1 | ForEach-Object { ($_ -split '\+')[0] })")
        }
        Invoke-Checked $wix @("build", "-arch", "x64", "-ext", "WixToolset.UI.wixext",
            "-d", "Version=$version", "-d", "SourceDir=$stage", "-d", "LicenseRtf=$rtf",
            "-d", "IconFile=$AppDir\resources\dragonunpacker.ico",
            "-o", $out, (Join-Path $wixDir "dragonunpacker.wxs"))
        Write-Host "msi: $out"
    } finally {
        $script:InstallPrefix = $saved
        Remove-Item -Recurse -Force (Split-Path $stage -Parent) -ErrorAction SilentlyContinue
    }
}

# ---- command line -----------------------------------------------------------------------------------------------------

if (-not $Action -or $Action -in @("-h", "--help", "help", "-?")) {
    Show-Usage
    exit $(if ($Action) { 0 } else { 1 })
}

$script:Reconfigure = $false
$Target = ""
$Positional = @()
$Pass = @()
$tokens = @($Rest)
$i = 0
if (($Action -eq "build" -or $Action -eq "rebuild") -and $tokens.Count -gt 0 -and $tokens[0] -notmatch '^-') {
    $Target = $tokens[0]
    $i = 1
}
while ($i -lt $tokens.Count) {
    $t = $tokens[$i]
    switch -Regex ($t) {
        '^--$' { $Pass = @($tokens[($i + 1)..($tokens.Count - 1)] | Where-Object { $null -ne $_ }); $i = $tokens.Count; continue }
        '^(-p|--prefix)$' {
            if ($i + 1 -ge $tokens.Count) { Fail "$t requires an argument" }
            $InstallPrefix = [IO.Path]::GetFullPath((Join-Path (Get-Location) $tokens[$i + 1])); $i += 2; continue
        }
        '^(-j|--jobs)$' {
            if ($i + 1 -ge $tokens.Count) { Fail "$t requires an argument" }
            $Jobs = [int]$tokens[$i + 1]; $i += 2; continue
        }
        '^(-B|--rebuild-all)$' { $Rebuild = $true; $i++; continue }
        '^(-v|--verbose)$' { $Verbose = $true; $i++; continue }
        '^(-h|--help)$' { Show-Usage; exit 0 }
        '^-' {
            # for "run", anything that looks like an option belongs to the application
            if ($Action -eq "run") { $Pass += $t; $i++; continue }
            Fail "unknown option: $t"
        }
        default { $Positional += $t; $i++; continue }
    }
}

switch ($Action) {
    "configure" { Do-Configure }
    "build" { Do-Build $Target }
    "rebuild" { $Rebuild = $true; Do-Build $Target }
    "run" { Do-Run ($Positional + $Pass) }
    "test" { Do-Test }
    "test-driver" { Do-TestDriver }
    "test-ui" { Do-TestUi }
    "e2e" { Do-E2e $Positional }
    "sweep" { Do-Sweep $Positional }
    "bench" { Do-Bench $Positional }
    "list" { Do-List $Positional }
    "extract" { Do-Extract $Positional }
    "clean" { Do-Clean }
    "purge" { Do-Purge }
    "declutter" { Do-Declutter }
    "install" { Do-Install }
    "package" { Do-Package }
    "msi" { Do-Msi }
    default { Fail "unknown action: $Action (see .\make.ps1 --help)" }
}
