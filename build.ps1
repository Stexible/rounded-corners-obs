$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Work = Join-Path $Root '_build-work'
$Dist = Join-Path $Root 'dist'

if (Test-Path $Work) { Remove-Item $Work -Recurse -Force }
if (Test-Path $Dist) { Remove-Item $Dist -Recurse -Force }
New-Item -ItemType Directory -Path $Work, $Dist | Out-Null

Write-Host 'Downloading official OBS plugin template...'
$Zip = Join-Path $Root 'obs-plugintemplate.zip'
Invoke-WebRequest -Uri 'https://github.com/obsproject/obs-plugintemplate/archive/refs/heads/master.zip' -OutFile $Zip
Expand-Archive -Path $Zip -DestinationPath $Work -Force
$Template = Join-Path $Work 'obs-plugintemplate-master'
if (!(Test-Path (Join-Path $Template 'CMakeLists.txt'))) { throw 'Could not locate the extracted OBS plugin template.' }

Copy-Item (Join-Path $Root 'src/plugin-main.c') (Join-Path $Template 'src/plugin-main.c') -Force
$EffectDir = Join-Path $Template 'data/effects'
New-Item -ItemType Directory -Path $EffectDir -Force | Out-Null
Copy-Item (Join-Path $Root 'data/effects/rounded-corners.effect') $EffectDir -Force
Copy-Item (Join-Path $Root 'data/locale/en-US.ini') (Join-Path $Template 'data/locale/en-US.ini') -Force
Copy-Item (Join-Path $Root 'data/locale/ru-RU.ini') (Join-Path $Template 'data/locale/ru-RU.ini') -Force

$PresetPath = Join-Path $Template 'CMakePresets.json'
$Presets = Get-Content $PresetPath -Raw | ConvertFrom-Json

$WindowsPreset = $Presets.configurePresets | Where-Object { $_.name -eq 'windows-x64' }
$WindowsPreset.generator = 'Visual Studio 18 2026'
$WindowsPreset.architecture = 'x64,version=10.0.26100.0'
$WindowsPreset.binaryDir = '${sourceDir}/build_x64'

$PresetsJson = $Presets | ConvertTo-Json -Depth 50
[System.IO.File]::WriteAllText($PresetPath, $PresetsJson, (New-Object System.Text.UTF8Encoding($false)))
$SpecPath = Join-Path $Template 'buildspec.json'
$Spec = Get-Content $SpecPath -Raw | ConvertFrom-Json
$Spec.name = 'rounded-corners'
$Spec.displayName = 'Rounded Corners for OBS'
$Spec.version = '1.0.0'
$Spec.author = 'Rounded Corners community project'
$Spec.website = 'https://github.com/obsproject/obs-plugintemplate'
$Spec.email = 'noreply@example.com'
$SpecJson = $Spec | ConvertTo-Json -Depth 20
[System.IO.File]::WriteAllText($SpecPath, $SpecJson, (New-Object System.Text.UTF8Encoding($false)))

Push-Location $Template
try {
    Write-Host 'Configuring Windows x64 build. First run downloads OBS build dependencies and may take several minutes.'
    cmake --preset windows-x64
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
    cmake --build --preset windows-x64 --config Release
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
    cmake --install build_x64 --config Release --prefix $Dist
    if ($LASTEXITCODE -ne 0) { throw 'Install staging failed.' }
}
finally { Pop-Location }


# Prepare ZIP with the exact OBS plugin directory structure.
$Package = Join-Path $Root 'rounded-corners-windows-x64-plugin.zip'
$Stage = Join-Path $Root '_package-staging'

if (Test-Path $Stage) { Remove-Item $Stage -Recurse -Force }
New-Item -ItemType Directory -Path `
    (Join-Path $Stage 'obs-plugins/64bit'), `
    (Join-Path $Stage 'data/obs-plugins/rounded-corners/effects'), `
    (Join-Path $Stage 'data/obs-plugins/rounded-corners/locale') `
    -Force | Out-Null

# Locate the compiled plugin DLL in the install staging directory.
$Dll = Get-ChildItem -Path $Dist -Filter 'rounded-corners.dll' -Recurse |
    Select-Object -First 1

if (!$Dll) {
    throw "Could not find rounded-corners.dll under $Dist"
}

Copy-Item $Dll.FullName `
    (Join-Path $Stage 'obs-plugins/64bit/rounded-corners.dll') -Force

# Copy data files directly from the source tree to the required ZIP paths.
Copy-Item (Join-Path $Root 'data/effects/rounded-corners.effect') `
    (Join-Path $Stage 'data/obs-plugins/rounded-corners/effects/rounded-corners.effect') -Force

Copy-Item (Join-Path $Root 'data/locale/en-US.ini') `
    (Join-Path $Stage 'data/obs-plugins/rounded-corners/locale/en-US.ini') -Force

Copy-Item (Join-Path $Root 'data/locale/ru-RU.ini') `
    (Join-Path $Stage 'data/obs-plugins/rounded-corners/locale/ru-RU.ini') -Force

if (Test-Path $Package) { Remove-Item $Package -Force }

Compress-Archive -Path (Join-Path $Stage '*') `
    -DestinationPath $Package -CompressionLevel Optimal

Write-Host ''
Write-Host "Build finished. Plugin ZIP: $Package"
Write-Host 'ZIP structure:'
Get-ChildItem $Stage -Recurse -File |
    ForEach-Object { Write-Host ($_.FullName.Substring($Stage.Length + 1)) }

Write-Host ''
Write-Host 'Extract the ZIP into your OBS installation directory.'
