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
Push-Location $projectRoot
try {
    New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
    $sources = @(
        "src/main.c", "src/System.c", "src/MainScreenSystem.c", "src/AlbumSystem.c",
        "src/WhiteboardSystem.c", "src/GameSystem.c", "src/GamesMenuSystem.c",
        "src/GomokuSystem.c", "src/ToeSystem.c", "src/ChessSystem.c", "src/boardgames.c",
        "src/LoginSystem.c", "src/game.c",
        "src/system/lcd.c",
        "src/system/file_manager.c", "src/system/print.c",
        "src/system/touch_cal.c", "src/system/calibrate.c", "src/system/platform.c"
    )
    # ARMv7-A soft-float EABI runs in the GEC6818's 32-bit Linux environment.
    # musl is linked statically, avoiding a dependency on the board's glibc version.
    $common = @("cc", "-target", "arm-linux-musleabi", "-mcpu=cortex_a9", "-std=c99", "-O2", "-Iinc", "-Iinc/system")
    & $Zig @common -c src/system/truetype.c -o "$OutputDirectory/truetype.o"
    if ($LASTEXITCODE -ne 0) { throw "ARM font compilation failed" }
    & $Zig @common -Wall -Wextra -Wpedantic -Werror -Wvla -Wformat=2 @sources "$OutputDirectory/truetype.o" -static -lm -o "$OutputDirectory/terminal"
    if ($LASTEXITCODE -ne 0) { throw "ARM application compilation failed" }
    Write-Output "Built $OutputDirectory/terminal (ARMv7-A, EABI soft-float, static musl; board validation still required)."
} finally {
    Pop-Location
}
