[CmdletBinding()]
param(
    [string] $JucePath,
    [string] $CMakePath,
    [string] $VisualStudioGenerator,
    [string] $OutputDirectory = 'build-release',
    [switch] $BootstrapJuce,
    [switch] $Clean
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path -LiteralPath $PSScriptRoot).Path
$outputPath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $OutputDirectory))
$repoPrefix = $repoRoot.TrimEnd('\') + '\'
if (-not $outputPath.StartsWith($repoPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "OutputDirectory must be inside this repository: '$outputPath'."
}

function Find-CMake {
    param([string] $ExplicitPath)

    if ($ExplicitPath) {
        $candidate = (Resolve-Path -LiteralPath $ExplicitPath).Path
        if ((Get-Item -LiteralPath $candidate).PSIsContainer) {
            $candidate = Join-Path $candidate 'cmake.exe'
        }
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
        throw "CMake was not found at -CMakePath '$ExplicitPath'."
    }

    $command = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }

    $candidates = @(
        (Join-Path ${env:ProgramFiles} 'CMake\bin\cmake.exe'),
        (Join-Path ${env:ProgramFiles(x86)} 'CMake\bin\cmake.exe')
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
    }
    throw 'CMake 3.22+ was not found. Install CMake, add it to PATH, or pass -CMakePath <path-to-cmake.exe>.'
}

function Find-VisualStudioGenerator {
    param([string] $ExplicitGenerator)

    if ($ExplicitGenerator) { return $ExplicitGenerator }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
        throw 'Visual Studio C++ tools were not detected (vswhere.exe missing). Install Visual Studio Build Tools with Desktop development with C++, or pass -VisualStudioGenerator "Visual Studio 17 2022".'
    }

    $versions = @(& $vswhere -all -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion)
    if ($LASTEXITCODE -ne 0) { throw 'vswhere failed while searching for Visual Studio C++ build tools.' }
    foreach ($version in ($versions | Sort-Object -Descending)) {
        if ($version -match '^(\d+)\.') {
            switch ([int]$Matches[1]) {
                17 { return 'Visual Studio 17 2022' }
                16 { return 'Visual Studio 16 2019' }
            }
        }
    }
    throw "No supported Visual Studio generator was found ($($versions -join ', ')). Install VS 2022/2019 C++ tools, or pass -VisualStudioGenerator explicitly."
}

function Find-JuceRoot {
    param([string] $ExplicitPath)

    $candidates = @()
    if ($ExplicitPath) { $candidates += $ExplicitPath }
    if ($env:JUCE_PATH) { $candidates += $env:JUCE_PATH }
    $candidates += @(
        (Join-Path $outputPath '_deps\JUCE'),
        (Join-Path (Split-Path $repoRoot -Parent) 'JUCE-8.0.15'),
        (Join-Path (Split-Path $repoRoot -Parent) 'JUCE'),
        (Join-Path $repoRoot 'JUCE')
    )

    foreach ($candidate in ($candidates | Select-Object -Unique)) {
        if ($candidate -and (Test-Path -LiteralPath (Join-Path $candidate 'modules\CMakeLists.txt') -PathType Leaf)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    if (-not $BootstrapJuce) {
        throw 'JUCE source was not found. Pass -JucePath <JUCE-root>, set JUCE_PATH, or use -BootstrapJuce to clone JUCE 8.0.15 into the ignored build directory.'
    }

    $git = Get-Command git.exe -ErrorAction SilentlyContinue
    if (-not $git) { throw 'git.exe is required for -BootstrapJuce but was not found on PATH.' }
    $dependencies = Join-Path $outputPath '_deps'
    $juceDestination = Join-Path $dependencies 'JUCE'
    New-Item -ItemType Directory -Path $dependencies -Force | Out-Null
    if (-not (Test-Path -LiteralPath (Join-Path $juceDestination 'modules\CMakeLists.txt') -PathType Leaf)) {
        if (Test-Path -LiteralPath $juceDestination) { Remove-Item -LiteralPath $juceDestination -Recurse -Force }
        & $git.Source clone --depth 1 --branch 8.0.15 https://github.com/juce-framework/JUCE.git $juceDestination
        if ($LASTEXITCODE -ne 0) { throw "Cloning JUCE 8.0.15 failed with exit code $LASTEXITCODE." }
    }
    return (Resolve-Path -LiteralPath $juceDestination).Path
}

$cmake = Find-CMake -ExplicitPath $CMakePath
$cmakeVersionText = (& $cmake --version | Select-Object -First 1)
if ($cmakeVersionText -notmatch 'version\s+(\d+)\.(\d+)') { throw "Cannot parse CMake version: '$cmakeVersionText'." }
if ([int]$Matches[1] -lt 3 -or ([int]$Matches[1] -eq 3 -and [int]$Matches[2] -lt 22)) {
    throw "CMake 3.22 or newer is required; found '$cmakeVersionText'."
}
$generator = Find-VisualStudioGenerator -ExplicitGenerator $VisualStudioGenerator

if ($Clean -and (Test-Path -LiteralPath $outputPath)) {
    try { Remove-Item -LiteralPath $outputPath -Recurse -Force -ErrorAction Stop }
    catch { throw "Could not clean '$outputPath'. Close programs using the build output and retry. Details: $($_.Exception.Message)" }
}
$juceRoot = Find-JuceRoot -ExplicitPath $JucePath
New-Item -ItemType Directory -Path $outputPath -Force | Out-Null

$pluginBundle = Join-Path $outputPath 'RetroSurround_artefacts\Release\VST3\RetroSurround.vst3'
$pluginBinary = Join-Path $pluginBundle 'Contents\x86_64-win\RetroSurround.vst3'
Write-Host "CMake: $cmakeVersionText ($cmake)"
Write-Host "Generator: $generator, x64"
Write-Host "JUCE: $juceRoot"
Write-Host 'Configuration: Release'

Push-Location $repoRoot
try {
    & $cmake -S . -B $outputPath -G $generator -A x64 "-DJUCE_SOURCE_DIR=$juceRoot"
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed with exit code $LASTEXITCODE." }

    & $cmake --build $outputPath --config Release --target RetroSurround_VST3
    if ($LASTEXITCODE -ne 0) { throw "CMake Release build failed with exit code $LASTEXITCODE." }
    if (-not (Test-Path -LiteralPath $pluginBinary -PathType Leaf)) {
        throw "Release build succeeded but its x64 VST3 module is missing: '$pluginBinary'."
    }

    $file = Get-Item -LiteralPath $pluginBinary
    Write-Host "Release VST3 build succeeded: $pluginBundle" -ForegroundColor Green
    Write-Host "Binary: $($file.Length) bytes, modified $($file.LastWriteTime)."
}
finally {
    Pop-Location
}
