# FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
# Optional inference libraries and model. All payloads are pinned and kept per user.
[CmdletBinding()]param()
$ErrorActionPreference='Stop'
$base=Join-Path $env:LOCALAPPDATA 'FrameBridge'
$cache=Join-Path $base 'Tools\Packages'
$runtime=Join-Path $base 'AI\1.24.4'
$model=Join-Path $base 'Models\RIFE-4.9'
foreach($d in @($cache,$runtime,$model)){[IO.Directory]::CreateDirectory($d)|Out-Null}
Add-Type -AssemblyName System.IO.Compression.FileSystem
function Fetch($url,$dest,$hash,$algo){
    if((Test-Path -LiteralPath $dest) -and (Get-FileHash -LiteralPath $dest -Algorithm $algo).Hash -eq $hash){return}
    $temp=$dest+'.'+[guid]::NewGuid().ToString('N')+'.tmp'
    Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $temp
    if((Get-FileHash -LiteralPath $temp -Algorithm $algo).Hash -ne $hash){throw "Integrity check failed for $url"}
    Move-Item -Force -LiteralPath $temp -Destination $dest
}
function Package($id,$version,$hash,$entries){
    $name="$id.$version.nupkg"
    $pkg=Join-Path $cache $name
    Fetch "https://api.nuget.org/v3-flatcontainer/$id/$version/$name" $pkg $hash SHA512
    $archive=[IO.Compression.ZipFile]::OpenRead($pkg)
    try{foreach($key in $entries.Keys){
        $dest=Join-Path $runtime $entries[$key]
        [IO.Compression.ZipFileExtensions]::ExtractToFile($archive.GetEntry($key),$dest,$true)
    }}finally{$archive.Dispose()}
}
Package 'microsoft.ml.onnxruntime.directml' '1.24.4' '6633B2BF8F79BE17D55E84C9A76BAD4729FC8ABD53148BC28F407D24FF106460574B4A05C7E958ED9099A927675528505FA12DC588B64F754EB585DC9814D5E0' @{
    'runtimes/win-x64/native/onnxruntime.dll'='onnxruntime.dll'
    'runtimes/win-x64/native/onnxruntime_providers_shared.dll'='onnxruntime_providers_shared.dll'
    'LICENSE'='ONNXRuntime-LICENSE.txt'
    'ThirdPartyNotices.txt'='ONNXRuntime-ThirdPartyNotices.txt'
}
Package 'microsoft.ai.directml' '1.15.4' 'FDE767F56904ABC90FD53F65D8729C918AB7F6E3C5E1ECDD479908FC02B4535CF2B0860F7AB2ACB9B731D6CB809B72C3D5D4D02853FB8F5EA022A47BC44EF285' @{
    'bin/x64-win/DirectML.dll'='DirectML.dll'
    'LICENSE.txt'='DirectML-LICENSE.txt'
    'ThirdPartyNotices.txt'='DirectML-ThirdPartyNotices.txt'
}
$url='https://huggingface.co/edgetools/rife/resolve/321b593c5f36c26f5b08fa2838dcf5eece382fa0'
Fetch "$url/rife49.onnx" (Join-Path $model 'rife49.onnx') '76E4CEF9AB42FA7DD4E8F6E4ABA47462051E3FAA969E4BCA6479784FBAB0AC6F' SHA256
Invoke-WebRequest -UseBasicParsing "$url/LICENSE" -OutFile (Join-Path $model 'LICENSE')
Invoke-WebRequest -UseBasicParsing "$url/README.md" -OutFile (Join-Path $model 'ModelCard.md')
Write-Host "Installed ONNX Runtime, DirectML and RIFE 4.9 under $base."
