param([Parameter(Mandatory)][string]$Name)
# Pull the logs of the previous iOS boot from a Galaxy S8 in TWRP:
#  - the kernel's on-disk console ring (last 1025 blocks of USERDATA, sda24),
#    decoded with tools/sweet-log.py -> logs/sweetlog-NAME.txt
#  - the loader + early kernel text in ramoops pmsg -> logs/pmsg-NAME.txt
# Usage: tools\dreamlte\pull-sweetlog.ps1 v25
$ErrorActionPreference = 'Stop'
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$dir = Join-Path $repo 'logs'
New-Item -ItemType Directory -Force $dir | Out-Null
$bin = Join-Path $dir "sweetlog-$Name.bin"
$txt = Join-Path $dir "sweetlog-$Name.txt"
$pmsg = Join-Path $dir "pmsg-$Name.txt"
$state = (adb get-state 2>$null)
if ($state -ne 'recovery') { throw "Expected TWRP recovery; ADB state is '$state'" }
$dev = (adb shell 'readlink -f /dev/block/platform/11120000.ufs/by-name/USERDATA').Trim()
if ($dev -notmatch '^/dev/block/sd[a-z]+[0-9]+$') { throw "Could not resolve USERDATA (got '$dev')" }
$size = [int64](adb shell "blockdev --getsize64 $dev").Trim()
$blocks = [int64][math]::Floor($size / 4096)
if ($blocks -le 1025) { throw "USERDATA is too small to contain the SWEETLOG area ($size bytes)" }
$skip = $blocks - 1025
adb shell "dd if=$dev of=/tmp/slog.bin bs=4096 skip=$skip count=1025 2>/dev/null"
if ($LASTEXITCODE -ne 0) { throw "Failed to read the USERDATA SWEETLOG area from $dev" }
adb pull /tmp/slog.bin $bin | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Failed to pull the USERDATA SWEETLOG area' }
adb pull /sys/fs/pstore/pmsg-ramoops-0 $pmsg | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Failed to pull pstore pmsg' }
python (Join-Path $repo 'tools\sweet-log.py') $bin $txt
if ($LASTEXITCODE -ne 0) { throw 'SWEETLOG decoder rejected the USERDATA area' }
Write-Output "kernel log: $txt"
Write-Output "pmsg log:   $pmsg"
