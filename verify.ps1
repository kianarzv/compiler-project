param(
    [ValidateNotNullOrEmpty()]
    [string]$BuildDirectory = "build/verify"
)

$ErrorActionPreference = "Stop"

# A separate Release build checks the complete project without depending on
# the interactive build cache. No existing build directory is deleted.
Push-Location $PSScriptRoot
try {
    cmake -S . -B $BuildDirectory -G Ninja -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    cmake --build $BuildDirectory
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    # Running from the build directory also works with older CTest versions.
    # Invalid sample programs are expected cases inside the tests; a nonzero
    # CTest status means an actual regression rather than a sample diagnostic.
    cmake -E chdir $BuildDirectory ctest --output-on-failure
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    $reviewDirectory = Join-Path $BuildDirectory "phase1-review"
    Write-Output "Phase one verification passed."
    Write-Output "Review summary: $(Join-Path $reviewDirectory 'summary.md')"
    Write-Output "Browser preview: $(Join-Path $reviewDirectory 'html_preview/highlighted.html')"
} finally {
    Pop-Location
}

exit 0
