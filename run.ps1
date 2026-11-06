param(
    [string]$Source = "examples/hello.bminor",
    [switch]$Tokens,
    [switch]$Ast,
    [switch]$Classify,
    [switch]$Ansi,
    [switch]$Html,
    [string]$Output = "",
    [switch]$ShowWhitespace
)

$ErrorActionPreference = "Stop"
$runExitCode = 1

# Resolve relative paths from the project directory, even when called elsewhere.
# finally restores the caller's directory on both success and failure.
Push-Location $PSScriptRoot
try {
    cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=g++
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    cmake --build build
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    # Forward each argument separately so paths containing spaces stay intact.
    $programArgs = @()
    if ($Tokens) { $programArgs += "--tokens" }
    if ($Ast) { $programArgs += "--ast" }
    if ($Classify) { $programArgs += "--classify" }
    if ($Ansi) { $programArgs += "--ansi" }
    if ($Html) {
        $programArgs += "--html"
        if (-not $Output) { $Output = "output/highlight.html" }
        # The native CLI writes binary HTML directly, avoiding PowerShell's
        # text-pipeline encoding. Create its parent for a ready-to-open result.
        $outputDirectory = Split-Path -Path $Output -Parent
        if ($outputDirectory) {
            New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
        }
    }
    if ($Output) { $programArgs += "--output"; $programArgs += $Output }
    if ($ShowWhitespace) { $programArgs += "--show-whitespace" }
    $programArgs += "--"
    $programArgs += $Source
    & "$PSScriptRoot/build/bminor.exe" @programArgs
    $runExitCode = $LASTEXITCODE
} finally {
    Pop-Location
}

exit $runExitCode
