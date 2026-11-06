param(
    [string]$Source = "examples/hello.bminor",
    [switch]$Tokens,
    [switch]$Ast,
    [switch]$Classify,
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
    if ($ShowWhitespace) { $programArgs += "--show-whitespace" }
    $programArgs += "--"
    $programArgs += $Source
    & "$PSScriptRoot/build/bminor.exe" @programArgs
    $runExitCode = $LASTEXITCODE
} finally {
    Pop-Location
}

exit $runExitCode
