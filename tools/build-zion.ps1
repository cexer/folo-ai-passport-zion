param([string]$BuildDirectory='build/zion', [switch]$TestControl, [string[]]$ExtraDefaults=@())
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(-not $env:IDF_PATH -or -not $env:IDF_PYTHON_ENV_PATH) {
    throw 'Activate ESP-IDF 5.5.3 before running tools/build-zion.ps1.'
}
$python=Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts/python.exe'
if(-not (Test-Path -LiteralPath $python)) {$python=Join-Path $env:IDF_PYTHON_ENV_PATH 'bin/python'}
$idf=Join-Path $env:IDF_PATH 'tools/idf.py'
function Invoke-ZionNative([string]$File,[string[]]$Arguments) {
    # Windows PowerShell wraps informational native stderr in NativeCommandError.
    # Preserve all output and judge the process by its real exit code.
    $savedPreference=$ErrorActionPreference
    try {$ErrorActionPreference='Continue'; & $File @Arguments; $code=$LASTEXITCODE}
    finally {$ErrorActionPreference=$savedPreference}
    if($code -ne 0) {throw "Zion build command failed with exit code $code"}
}
Push-Location $root
try {
    $env:SDKCONFIG_DEFAULTS=Join-Path $root 'sdkconfig.defaults'
    if($TestControl) {$env:SDKCONFIG_DEFAULTS+=';'+(Join-Path $root 'sdkconfig.zion-test.defaults')}
    foreach($overlay in $ExtraDefaults) {
        $overlayPath=if([IO.Path]::IsPathRooted($overlay)) {$overlay}else{Join-Path $root $overlay}
        if(-not (Test-Path -LiteralPath $overlayPath -PathType Leaf)) {throw "Missing defaults overlay: $overlayPath"}
        $env:SDKCONFIG_DEFAULTS+=';'+$overlayPath
    }
    Invoke-ZionNative $python @($idf,'-B',$BuildDirectory,'-D',"SDKCONFIG=$BuildDirectory/sdkconfig",'build')
    # A cached sdkconfig can silently override a newly selected overlay.
    # Reject that drift instead of benchmarking or delivering the wrong profile.
    $effectiveConfig=Get-Content -LiteralPath (Join-Path $BuildDirectory 'sdkconfig') -Raw
    foreach($overlay in $ExtraDefaults) {
        $overlayPath=if([IO.Path]::IsPathRooted($overlay)) {$overlay}else{Join-Path $root $overlay}
        foreach($line in Get-Content -LiteralPath $overlayPath) {
            if($line -match '^(CONFIG_[A-Z0-9_]+)=(.+)$') {
                $expected=if($Matches[2] -eq 'n') {"# $($Matches[1]) is not set"} else {$line}
                if($effectiveConfig.Split("`n").Trim() -notcontains $expected) {
                    throw "Overlay not effective: $line. Use a fresh build directory to regenerate sdkconfig."
                }
            }
        }
    }
    $merged=Join-Path (Join-Path $root $BuildDirectory) 'FoloToy-AI-Passport-full.bin'
    Invoke-ZionNative $python @($idf,'-B',$BuildDirectory,'merge-bin','-o',$merged)
    Invoke-ZionNative $python @((Join-Path $PSScriptRoot 'package_zion.py'),'--build-dir',$BuildDirectory)
} finally {Pop-Location}
