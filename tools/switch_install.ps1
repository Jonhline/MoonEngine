param([string]$PackageRoot = (Split-Path $PSScriptRoot), [switch]$SelfTest)
$ErrorActionPreference = 'Stop'
Add-Type -Path (Join-Path $PSScriptRoot 'switch_transfer.cs')

function Assert-Hash($Path, $Expected, $Algorithm = 'SHA256') {
    if ((Get-FileHash -LiteralPath $Path -Algorithm $Algorithm).Hash -ne $Expected) {
        throw "Arquivo diferente do esperado: $Path"
    }
}

function Get-SdFolder($Shell) {
    $matches = @()
    foreach ($device in $Shell.Namespace(17).Items()) {
        if (-not $device.IsFolder -or $device.IsFileSystem) { continue }
        foreach ($item in $device.GetFolder.Items()) {
            # DBI's SD storage only. Never choose NAND or an install endpoint.
            if ($item.Name -eq '1: SD Card' -and $item.IsFolder) { $matches += $item.GetFolder }
        }
    }
    if ($matches.Count -gt 1) { throw 'Conecte apenas um Switch por vez.' }
    if ($matches.Count -eq 1) { return $matches[0] }
    return $null
}

function Get-ChildFolder($Folder, $Name, [switch]$Create) {
    $item = $Folder.ParseName($Name)
    if (-not $item -and $Create) {
        $Folder.NewFolder($Name)
        for ($i = 0; $i -lt 40 -and -not $item; $i++) {
            Start-Sleep -Milliseconds 250
            $item = $Folder.ParseName($Name)
        }
    }
    if ($item -and $item.IsFolder) { return $item.GetFolder }
    if ($Create) { throw "Nao foi possivel criar a pasta $Name. Reconecte o DBI." }
    return $null
}

function Select-AppFolder($SwitchFolder) {
    # Match switch_startup.c: prefer the current folder; keep legacy saves in place.
    $current = Get-ChildFolder $SwitchFolder 'moonengine'
    if ($current) { return @($current, 'moonengine.nro', 'moonengine') }
    $legacy = Get-ChildFolder $SwitchFolder 'opengt4'
    if ($legacy) { return @($legacy, 'opengt4.nro', 'opengt4') }
    return @((Get-ChildFolder $SwitchFolder 'moonengine' -Create), 'moonengine.nro', 'moonengine')
}

function Set-DiscConfig([string]$Text, [string]$Directory) {
    $disc = "disc=sdmc:/switch/$Directory/gt4.iso"
    if ($Text -match '(?m)^disc=') { $Text = [regex]::Replace($Text, '(?m)^disc=[^\r\n]*', $disc) }
    else { $Text = $disc + "`n" + $Text }
    if ($Text -notmatch '(?m)^saves=') { $Text += "`nsaves=sdmc:/switch/$Directory/`n" }
    if ($Text -notmatch '(?m)^show=') { $Text += "`nshow=0`n" }
    return $Text.Replace("`r`n", "`n")
}

if ($SelfTest) {
    $temp = Join-Path ([IO.Path]::GetTempPath()) ('moon-transfer-' + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $temp | Out-Null
    $target = New-Item -ItemType Directory -Path (Join-Path $temp 'destination')
    $source = Join-Path $temp 'source.bin'
    [IO.File]::WriteAllBytes($source, [byte[]](0..255))
    [MoonEngine.Install.Transfer]::Copy($source, $target.FullName, 'renamed.bin', $true)
    Assert-Hash (Join-Path $target.FullName 'renamed.bin') (Get-FileHash $source).Hash
    $rejected = $false
    try { [MoonEngine.Install.Transfer]::Copy($source, $target.FullName, '../escape.bin', $true) }
    catch { $rejected = $true }
    if (-not $rejected) { throw 'Destination validation failed' }
    $shell = New-Object -ComObject Shell.Application
    $switch = New-Item -ItemType Directory -Path (Join-Path $temp 'switch')
    $legacy = New-Item -ItemType Directory -Path (Join-Path $switch.FullName 'opengt4')
    [IO.File]::WriteAllText((Join-Path $legacy.FullName 'slot01.ogt4'), 'preserve')
    $chosen = Select-AppFolder ($shell.Namespace($switch.FullName))
    if ($chosen[2] -ne 'opengt4') { throw 'Legacy folder selection failed' }
    New-Item -ItemType Directory -Path (Join-Path $switch.FullName 'moonengine') | Out-Null
    $chosen = Select-AppFolder ($shell.Namespace($switch.FullName))
    if ($chosen[2] -ne 'moonengine') { throw 'Current folder precedence failed' }
    if ([IO.File]::ReadAllText((Join-Path $legacy.FullName 'slot01.ogt4')) -ne 'preserve') {
        throw 'Save preservation failed'
    }
    $config = Set-DiscConfig "disc=old.iso`nsaves=sdmc:/custom/`nvsync=1`n" 'opengt4'
    if ($config -notmatch 'disc=sdmc:/switch/opengt4/gt4.iso' -or
        $config -notmatch 'saves=sdmc:/custom/' -or $config -notmatch 'vsync=1') {
        throw 'Existing preferences preservation failed'
    }
    Write-Output "PASS Shell copy/rename, hash, path rejection and save-folder precedence. Local fixture: $temp"
    exit 0
}

Add-Type -AssemblyName System.Windows.Forms
$title = 'Moon Engine - instalar no Switch'
$temp = Join-Path ([IO.Path]::GetTempPath()) ('moon-install-' + [Guid]::NewGuid().ToString('N'))
try {
    if ([Threading.Thread]::CurrentThread.ApartmentState -ne 'STA') { throw 'Execute Instalar no Switch.cmd.' }
    $manifest = Get-Content -LiteralPath (Join-Path $PackageRoot 'manifest.json') -Raw | ConvertFrom-Json
    $nro = Join-Path $PackageRoot 'switch/moonengine/moonengine.nro'
    Assert-Hash $nro $manifest.nro_sha256
    Write-Host '1/3 - Escolha sua copia do GT4 USA v2.00 (SCUS-97328).'
    $choose = New-Object Windows.Forms.OpenFileDialog
    $choose.Title = 'Escolha sua ISO do Gran Turismo 4 (USA v2.00)'
    $choose.Filter = 'Imagem de disco (*.iso)|*.iso'
    if ($choose.ShowDialog() -ne 'OK') { exit 0 }
    $iso = $choose.FileName
    Write-Host 'Conferindo a ISO. Isso pode levar alguns minutos...'
    Assert-Hash $iso $manifest.disc_sha1 'SHA1'
    $message = "2/3 - No Switch, abra DBI > Start MTP Connection e conecte o cabo USB.`n`n" +
               'O cartao precisa aceitar um arquivo de mais de 4 GB (exFAT). Clique OK quando estiver conectado.'
    if ([Windows.Forms.MessageBox]::Show($message, $title, 'OKCancel') -ne 'OK') { exit 0 }
    $shell = New-Object -ComObject Shell.Application
    $sd = Get-SdFolder $shell
    if (-not $sd) { throw 'DBI nao encontrado. Abra Start MTP Connection, confira o cabo e tente novamente.' }
    $selection = Select-AppFolder (Get-ChildFolder $sd 'switch' -Create)
    $destination, $nroName, $directory = $selection
    Write-Host "3/3 - Instalando em switch/$directory. Aguarde a janela de copia do Windows."
    New-Item -ItemType Directory -Path $temp | Out-Null
    $existing = $destination.ParseName('gt4.iso')
    if ($existing) {
        $answer = [Windows.Forms.MessageBox]::Show(
            'Ja existe gt4.iso. Sim = manter a copia do cartao; Nao = substituir pela ISO escolhida.',
            $title, 'YesNoCancel')
        if ($answer -eq 'Cancel') { exit 0 }
        if ($answer -eq 'No') { [MoonEngine.Install.Transfer]::Copy($iso, $destination.Self.Path, 'gt4.iso', $false) }
    } else { [MoonEngine.Install.Transfer]::Copy($iso, $destination.Self.Path, 'gt4.iso', $false) }
    $installedIso = $destination.ParseName('gt4.iso')
    if (-not $installedIso -or $installedIso.IsFolder -or
        [int64]$installedIso.ExtendedProperty('System.Size') -ne (Get-Item -LiteralPath $iso).Length) {
        throw 'A ISO no cartao nao tem o tamanho esperado. Confira o espaco livre e se o SD e exFAT.'
    }
    $existing = $destination.ParseName($nroName)
    if ($existing) {
        # Keep a recoverable copy outside hbmenu's NRO listing.
        $backups = Get-ChildFolder $sd 'moonengine-backups' -Create
        [MoonEngine.Install.Transfer]::Copy($existing.Path, $backups.Self.Path,
            ('before-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.nro.disabled'), $false)
    }
    [MoonEngine.Install.Transfer]::Copy($nro, $destination.Self.Path, $nroName, $false)
    # Verify the actual installed binary, rather than only the local package.
    $installed = $destination.ParseName($nroName)
    if (-not $installed) { throw 'O DBI nao confirmou o NRO. Reconecte e tente novamente.' }
    [MoonEngine.Install.Transfer]::Copy($installed.Path, $temp, 'verified.nro', $true)
    Assert-Hash (Join-Path $temp 'verified.nro') $manifest.nro_sha256
    $config = ''
    $existingConfig = $destination.ParseName('config.ini')
    if ($existingConfig) {
        [MoonEngine.Install.Transfer]::Copy($existingConfig.Path, $temp, 'old-config.ini', $true)
        $config = [IO.File]::ReadAllText((Join-Path $temp 'old-config.ini'))
        $backups = Get-ChildFolder $sd 'moonengine-backups' -Create
        [MoonEngine.Install.Transfer]::Copy($existingConfig.Path, $backups.Self.Path,
            ('config-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.ini.bak'), $false)
    }
    $config = Set-DiscConfig $config $directory
    [IO.File]::WriteAllText((Join-Path $temp 'config.ini'), $config, (New-Object Text.UTF8Encoding($false)))
    [MoonEngine.Install.Transfer]::Copy((Join-Path $temp 'config.ini'), $destination.Self.Path, 'config.ini', $false)
    [Windows.Forms.MessageBox]::Show(
        "Pronto! Encerre o MTP no DBI.`n`nSegure R ao abrir um jogo para entrar no Homebrew Menu com RAM completa e abra Moon Engine.`n`nSaves e configuracoes existentes foram preservados.", $title) | Out-Null
} catch {
    Write-Error $_ -ErrorAction Continue
    [Windows.Forms.MessageBox]::Show("Instalacao nao concluida.`n`n$($_.Exception.Message)`n`nNao apague saves. Reconecte o DBI e tente de novo.", $title) | Out-Null
    exit 1
}
