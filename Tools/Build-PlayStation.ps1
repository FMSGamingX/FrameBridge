# FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
[CmdletBinding()]param([string]$MsysRoot, [string]$Source, [switch]$Test)
$ErrorActionPreference='Stop'
$root=(Resolve-Path "$PSScriptRoot/..").Path
$tools=Join-Path $env:LOCALAPPDATA 'FrameBridge/Tools'
if(!$MsysRoot){$MsysRoot=Join-Path $tools 'msys64'}
if(!$Source){$Source=Join-Path $tools 'chiaki-ng-1.10.1'}
$pin='f30da0aaeb96fc0026cc03683153f682352e7d7c'
if(!(Test-Path "$Source/.git")){
    git clone --depth 1 --branch v1.10.1 https://github.com/chiaki-ng/chiaki-ng.git $Source
    if($LASTEXITCODE){throw 'Unable to fetch chiaki-ng'}
}
if((git -C $Source rev-parse HEAD) -ne $pin){throw 'Expected chiaki-ng v1.10.1 at its pinned revision.'}
git -C $Source submodule update --init --depth 1 third-party/gf-complete third-party/jerasure third-party/nanopb
if($LASTEXITCODE){throw 'Unable to fetch chiaki-ng submodules'}
$ucrt=Join-Path $MsysRoot 'ucrt64'
$build=Join-Path $tools 'build-ps5-1.10.1'
$env:PATH="$ucrt/bin;$env:PATH"
$env:PKG_CONFIG_PATH="$ucrt/lib/pkgconfig"
& "$ucrt/bin/cmake.exe" -S "$root/PlayStation" -B $build -G Ninja "-DCHIAKI_SOURCE=$Source" "-DCMAKE_PREFIX_PATH=$ucrt" "-DCMAKE_BUILD_TYPE=Release" "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
if($LASTEXITCODE){throw 'PS5 configuration failed'}
& "$ucrt/bin/cmake.exe" --build $build --target FrameBridge.PlayStation -j 6
if($LASTEXITCODE){throw 'PS5 module compilation failed'}
$dest=Join-Path $root 'bin/PlayStation'
New-Item -Force -ItemType Directory $dest | Out-Null
# Follow the PE import table and copy only the UCRT64 runtime dependencies.
$queue=New-Object 'Collections.Generic.Queue[string]'
$seen=@{}
$queue.Enqueue((Join-Path $build 'FrameBridge.PlayStation.dll'))
while($queue.Count){
    $file=$queue.Dequeue()
    $name=Split-Path $file -Leaf
    if($seen[$name]){continue}
    $seen[$name]=$true
    Copy-Item -LiteralPath $file -Destination $dest -Force
    $imports=& "$ucrt/bin/objdump.exe" -p $file
    if($LASTEXITCODE){throw "Cannot inspect $name"}
    foreach($line in $imports){
        if($line -match 'DLL Name:\s*(\S+)'){
            $dep=Join-Path "$ucrt/bin" $Matches[1]
            if(Test-Path -LiteralPath $dep){$queue.Enqueue($dep)}
        }
    }
}
Copy-Item "$Source/COPYING" "$dest/chiaki-ng-COPYING.txt" -Force
$notices=Join-Path $dest 'Notices'
New-Item -ItemType Directory -Force $notices | Out-Null
New-Item -ItemType Directory -Force "$notices/chiaki-ng" | Out-Null
Get-ChildItem "$Source/LICENSES" | Copy-Item -Destination "$notices/chiaki-ng" -Recurse -Force
foreach($library in 'jerasure','gf-complete','nanopb'){
    $to=Join-Path $notices $library
    New-Item -ItemType Directory -Force $to | Out-Null
    Get-ChildItem "$Source/third-party/$library" -File | Where-Object Name -match '^(COPYING|LICENSE)' |
        Copy-Item -Destination $to -Force
}
# Keep the upstream notices provided by the private build environment with its runtime.
New-Item -ItemType Directory -Force "$notices/UCRT64" | Out-Null
Get-ChildItem "$ucrt/share/licenses" | Copy-Item -Destination "$notices/UCRT64" -Recurse -Force
Write-Host "Private PS5 module built at $dest"
if($Test){
    & "$ucrt/bin/cmake.exe" --build $build --target PS5DecoderTest
    if($LASTEXITCODE){throw 'Decoder test compilation failed'}
    & "$ucrt/bin/ffmpeg.exe" -hide_banner -loglevel error -y -f lavfi -i 'color=red:size=320x180:rate=30' -frames:v 1 -c:v libx264 -preset ultrafast -tune zerolatency -f h264 "$build/test-red.h264"
    if($LASTEXITCODE){throw 'H.264 test fixture creation failed'}
    & "$build/PS5DecoderTest.exe" "$build/test-red.h264"
    if($LASTEXITCODE){throw "PS5 decoder regression failed ($LASTEXITCODE)"}
}
