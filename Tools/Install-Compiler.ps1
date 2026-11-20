# FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
$ErrorActionPreference='Stop'
$dir=Join-Path $env:LOCALAPPDATA 'FrameBridge\Tools'
[IO.Directory]::CreateDirectory($dir) | Out-Null
$name='llvm-mingw-20261117-ucrt-x86_64'
$file=Join-Path $dir ($name+'.zip')
if(!(Test-Path $file)){Invoke-WebRequest -UseBasicParsing "https://github.com/mstorsjo/llvm-mingw/releases/download/20261117/$name.zip" -OutFile $file}
if((Get-FileHash $file -Algorithm SHA256).Hash -ne '4D002E330A0F703625AA5CCBC37E96F8F824413E163BFC7336774AABD2C3A412'){throw 'Compiler archive checksum mismatch.'}
tar.exe -xf $file -C $dir
if($LASTEXITCODE){throw 'Compiler extraction failed'}
& (Join-Path $dir "$name\bin\clang++.exe") --version
