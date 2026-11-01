# Read-only Windows inventory for Cobalt64.
# This script does not read GPU BAR/MMIO, alter drivers, or flash firmware.
param([string]$OutFile)
$ErrorActionPreference = 'SilentlyContinue'
$gpu = Get-CimInstance Win32_VideoController | Where-Object { $_.PNPDeviceID -match 'VEN_1002&DEV_7480' } | Select-Object -First 1
$pnp = Get-PnpDevice -PresentOnly | Where-Object { $_.InstanceId -match 'VEN_1002&DEV_7480' } | Select-Object -First 1
$os = Get-CimInstance Win32_OperatingSystem
$props = @{}
if ($pnp) {
  Get-PnpDeviceProperty -InstanceId $pnp.InstanceId | ForEach-Object { $props[$_.KeyName] = $_.Data }
}
$hipCandidates = @()
$hipCandidates += Get-Command hipInfo.exe -ErrorAction SilentlyContinue | ForEach-Object { $_.Source }
foreach ($root in @('C:\Program Files\AMD\ROCm', 'C:\Program Files\AMD\HIP')) {
  if (Test-Path $root) { $hipCandidates += Get-ChildItem $root -Filter hipInfo.exe -Recurse -File -ErrorAction SilentlyContinue | ForEach-Object { $_.FullName } }
}
$hipCandidates = @($hipCandidates | Where-Object { $_ } | Select-Object -Unique)
$hipOutput = @()
foreach ($exe in $hipCandidates) {
  $hipOutput += "### $exe"
  $hipOutput += (& $exe 2>&1 | Out-String)
}
$report = [ordered]@{
  collectedAt = (Get-Date).ToString('o')
  operatingSystem = [ordered]@{ caption = $os.Caption; version = $os.Version; build = $os.BuildNumber }
  gpu = if ($gpu) {
    [ordered]@{
      name = $gpu.Name
      pnpDeviceId = $gpu.PNPDeviceID
      driverVersion = $gpu.DriverVersion
      driverDate = $gpu.DriverDate
      status = $pnp.Status
      locationInfo = $props['DEVPKEY_Device_LocationInfo']
      locationPaths = $props['DEVPKEY_Device_LocationPaths']
      busNumber = $props['DEVPKEY_Device_BusNumber']
      address = $props['DEVPKEY_Device_Address']
      hardwareIds = $props['DEVPKEY_Device_HardwareIds']
      service = $props['DEVPKEY_Device_Service']
    }
  } else { $null }
  hipInfoExecutables = $hipCandidates
  hipInfoOutput = $hipOutput
  collectionScope = 'Windows PnP/CIM metadata only; no PCI config-space reads, BAR/MMIO reads, VRAM dumps, or firmware writes.'
  interpretation = 'PCI identity and HIP architecture/runtime info are not a raw IP Discovery binary. This probe cannot validate macOS driver initialization.'
}
$json = $report | ConvertTo-Json -Depth 8
if ($OutFile) {
  $parent = Split-Path -Parent $OutFile
  if ($parent -and !(Test-Path $parent)) { New-Item -ItemType Directory -Path $parent -Force | Out-Null }
  $json | Set-Content -LiteralPath $OutFile -Encoding UTF8
}
$json
