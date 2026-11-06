param(
    [string]$Source = "examples/hello.bminor"
)

$ErrorActionPreference = "Stop"
$runExitCode = 1

Push-Location $PSScriptRoot
try {
    cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=g++
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    cmake --build build
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    & "$PSScriptRoot/build/bminor.exe" $Source
    $runExitCode = $LASTEXITCODE
} finally {
    Pop-Location
}

exit $runExitCode
