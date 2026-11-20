# FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
[CmdletBinding()]param([string]$Compiler,[switch]$Test)
$ErrorActionPreference='Stop'
Push-Location -LiteralPath $PSScriptRoot
try {
if(!$Compiler){ $Compiler=Join-Path $env:LOCALAPPDATA 'FrameBridge\Tools\llvm-mingw-20261117-ucrt-x86_64\bin\clang++.exe' }
if(!(Test-Path -LiteralPath $Compiler)){throw 'Install LLVM-MinGW (Documentation/Build.md), or provide -Compiler.'}
$out=Join-Path $PSScriptRoot 'bin'
[IO.Directory]::CreateDirectory($out) | Out-Null
$cache=Join-Path $out '.build'
[IO.Directory]::CreateDirectory($cache) | Out-Null
$h=Join-Path $cache 'ShaderBytecode.hpp'
$shaderFiles=@('Engine/GPU/FlowShader.hpp','Tools/CompileShaders.cpp')
if(!(Test-Path $h) -or ($shaderFiles | Where-Object {(Get-Item $_).LastWriteTimeUtc -gt (Get-Item $h).LastWriteTimeUtc})){
    & $Compiler -std=c++20 -O2 -static Tools/CompileShaders.cpp -ld3dcompiler -o "$cache\CompileShaders.exe"
    if($LASTEXITCODE){throw 'Shader compiler build failed'}
    $code=& "$cache\CompileShaders.exe"
    if($LASTEXITCODE){throw 'HLSL compilation failed'}
    [IO.File]::WriteAllLines($h,[string[]]$code)
}
$windres=Join-Path (Split-Path -Parent $Compiler) 'x86_64-w64-mingw32-windres.exe'
& $windres Engine/Version.rc -O coff -o "$cache/Version.obj"
if($LASTEXITCODE){throw 'Engine version metadata compilation failed'}
$src=@(Get-ChildItem Engine -Recurse -Filter *.cpp | ForEach-Object FullName)
& $Compiler -std=c++20 -O2 -static -shared -DUNICODE -D_UNICODE "-I$cache" -isystem ThirdParty/ONNXRuntime/include @src "$cache/Version.obj" -o "$out\FrameBridge.Engine.dll" -ld3d11 -ldxgi -ld3dcompiler -lmfreadwrite -lmfplat -lmf -lmfuuid -lole32 -luuid -lgdi32 -lwinmm -luser32 -lpdh
if($LASTEXITCODE){throw 'C++ compilation failed'}
$fx=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319'
$refs=@('WPF\WindowsBase','WPF\PresentationFramework','WPF\PresentationCore','System.Xaml','System.Windows.Forms','System.Drawing','System.Xml','System.Core','System.Configuration') | ForEach-Object {"/reference:$fx\$_.dll"}
$cs=@(Get-ChildItem App -Recurse -Filter *.cs | ForEach-Object FullName)
& "$fx\csc.exe" /nologo /utf8output /codepage:65001 /target:winexe /win32manifest:App/app.manifest /win32icon:Assets/FrameBridge.ico /platform:x64 /optimize+ "/out:$out\FrameBridge.exe" @refs /resource:App/Shell.xaml,Shell.xaml @cs
if($LASTEXITCODE){throw 'WPF compilation failed'}
Copy-Item -Recurse -Force 'ThirdParty/Notices' $out
Copy-Item -Force ThirdParty/ONNXRuntime/LICENSE "$out/Notices/ONNXRuntime-LICENSE.txt"
if($Test){
    & $Compiler -std=c++20 -O2 -static Tests/flow_test.cpp Engine/Interpolation/Flow.cpp -o "$cache/FlowTest.exe"
    if($LASTEXITCODE){throw 'Motion test compilation failed'}
    & "$cache/FlowTest.exe"
    if($LASTEXITCODE){throw 'Motion interpolation test failed'}
    & $Compiler -std=c++20 -O2 -static "-I$cache" Tests/gpu_flow_test.cpp Engine/Backends/Nvidia.cpp -ld3d11 -ldxgi -luuid -o "$cache/GpuFlowTest.exe"
    if($LASTEXITCODE){throw 'GPU regression compilation failed'}
    & "$cache/GpuFlowTest.exe"
    if($LASTEXITCODE){throw 'GPU midpoint or original preservation test failed'}
    $p=Start-Process "$out\FrameBridge.exe" -ArgumentList '--shell-test' -Wait -PassThru
    if($p.ExitCode){throw "Shell smoke test failed ($($p.ExitCode))"}
    Write-Host 'WPF shell and native ABI passed.'
}
Write-Host "$out\FrameBridge.exe"
} finally { Pop-Location }

