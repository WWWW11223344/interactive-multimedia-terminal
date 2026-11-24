param(
    [string]$Zig = "",
    [string]$OutputDirectory = "build-arm"
)
$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $Zig) {
    $localZig = Join-Path $projectRoot "../outputs/toolchains/zig-windows-x86_64-0.13.0/zig.exe"
    if (Test-Path -LiteralPath $localZig) { $Zig = (Resolve-Path -LiteralPath $localZig).Path }
    else { $Zig = (Get-Command zig -ErrorAction Stop).Source }
}

# Windows PowerShell 5.1 turns a native command's stderr into a terminating error when
# ErrorActionPreference is Stop, and zig writes ordinary progress notes to stderr. That
# combination once turned a failed compile into a "Built ..." line: the error fired on a
# progress note, the exit code was never read, and the previous binary stayed in place.
# So: no Stop while zig runs, capture its output, read $LASTEXITCODE immediately, and
# check that the file on disk is actually new.
function Invoke-Zig([string]$Label, [string[]]$Arguments, [string]$Product) {
    $before = if (Test-Path -LiteralPath $Product) { (Get-Item -LiteralPath $Product).LastWriteTime } else { $null }
    $output = & $Zig @Arguments 2>&1 | Out-String
    $code = $LASTEXITCODE
    if ($code -ne 0) {
        Write-Output $output.Trim()
        throw "$Label failed (exit $code)"
    }
    if (-not (Test-Path -LiteralPath $Product)) { throw "$Label reported success but produced no $Product" }
    $after = (Get-Item -LiteralPath $Product).LastWriteTime
    if ($null -ne $before -and $after -le $before) { throw "$Label left $Product unchanged; the build did not run" }
}

Push-Location $projectRoot
$previous = $ErrorActionPreference
$ErrorActionPreference = "Continue"
try {
    New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
    $sources = @(
        "src/main.c", "src/System.c", "src/MainScreenSystem.c", "src/AlbumSystem.c",
        "src/WhiteboardSystem.c", "src/GameSystem.c", "src/GamesMenuSystem.c",
        "src/GomokuSystem.c", "src/ToeSystem.c", "src/ChessSystem.c", "src/boardgames.c",
        "src/LoginSystem.c", "src/game.c",
        "src/system/lcd.c",
        "src/system/logger.c", "src/system/file_manager.c", "src/system/print.c",
        "src/system/touch_cal.c", "src/system/touch_probe.c", "src/system/touch_sweep.c",
        "src/system/calibrate.c", "src/system/platform.c"
    )
    # ARMv7-A soft-float EABI runs in the GEC6818's 32-bit Linux environment.
    # musl is linked statically, avoiding a dependency on the board's glibc version.
    $common = @("cc", "-target", "arm-linux-musleabi", "-mcpu=cortex_a9", "-std=c99", "-O2", "-Iinc", "-Iinc/system")
    # The font object is a build input, not a product, so it carries no freshness check.
    $fontOutput = & $Zig @common -c src/system/truetype.c -o "$OutputDirectory/truetype.o" 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { Write-Output $fontOutput.Trim(); throw "ARM font compilation failed" }
    Invoke-Zig "ARM application compilation" ($common + @("-Wall", "-Wextra", "-Wpedantic", "-Werror", "-Wvla", "-Wformat=2") + $sources + @("$OutputDirectory/truetype.o", "-static", "-lm", "-o", "$OutputDirectory/terminal")) "$OutputDirectory/terminal"
    Write-Output "Built $OutputDirectory/terminal (ARMv7-A, EABI soft-float, static musl; board validation still required)."
} finally {
    $ErrorActionPreference = $previous
    Pop-Location
}
