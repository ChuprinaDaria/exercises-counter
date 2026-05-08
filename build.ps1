$ErrorActionPreference = "Stop"
$Root = $PSScriptRoot

Write-Host "=== Installing Python dependencies ===" -ForegroundColor Cyan
pip install pybind11 scikit-build-core
pip install -e "$Root\.[web,dev]" --no-build-isolation

Write-Host "`n=== Configuring CMake ===" -ForegroundColor Cyan
$Pybind11Dir = python -c "import pybind11; print(pybind11.get_cmake_dir())"
cmake -S $Root -B "$Root\build_cpp" -DCMAKE_BUILD_TYPE=Release `
    "-Dpybind11_DIR=$Pybind11Dir" `
    -DEXCO_BUILD_TESTS=OFF `
    -Wno-dev

Write-Host "`n=== Building C++ extension ===" -ForegroundColor Cyan
cmake --build "$Root\build_cpp" --config Release

Write-Host "`n=== Copying extension to package ===" -ForegroundColor Cyan
$Pyd = Get-ChildItem -Path "$Root\build_cpp\cpp\Release" -Filter "_exco_cpp*.pyd" | Select-Object -First 1
if (-not $Pyd) {
    Write-Error "Build failed: _exco_cpp*.pyd not found in build_cpp\cpp\Release"
    exit 1
}
Copy-Item $Pyd.FullName "$Root\python\exco\$($Pyd.Name)" -Force
Write-Host "Installed: $($Pyd.Name)" -ForegroundColor Green

Write-Host "`n=== Verifying import ===" -ForegroundColor Cyan
python -c "import exco._exco_cpp as cpp; print('OK:', [x for x in dir(cpp) if not x.startswith('_')])"

Write-Host "`n=== Done ===" -ForegroundColor Green
