param(
    [ValidateSet('L1', 'L2', 'L3')]
    [string]$Level = $(if ($env:UBAANEXT_LIVE_LEVEL) { $env:UBAANEXT_LIVE_LEVEL } else { 'L1' }),
    [string]$CliPath = $(if ($env:UBAANEXT_CLI_PATH) { $env:UBAANEXT_CLI_PATH } elseif (Test-Path '.\bin\x64\Debug\ubaa.exe') { '.\bin\x64\Debug\ubaa.exe' } else { '.\build\windows-ninja-msvc-debug\apps\cli\ubaa.exe' }),
    [string]$Mode = $(if ($env:UBAANEXT_CONNECTION_MODE) { $env:UBAANEXT_CONNECTION_MODE } else { 'direct' })
)

$ErrorActionPreference = 'Stop'

function Redact-Text([string]$Text) {
    if ($null -eq $Text) { return '' }
    $redacted = $Text
    foreach ($value in @($env:UBAANEXT_USERNAME, $env:UBAANEXT_PASSWORD, $env:UBAANEXT_TOKEN, $env:UBAANEXT_COOKIE, $env:UBAANEXT_TICKET, $env:UBAANEXT_SESSION)) {
        if (![string]::IsNullOrWhiteSpace($value)) {
            $redacted = $redacted.Replace($value, '[REDACTED]')
        }
    }
    $redacted = [regex]::Replace($redacted, '(?i)(https?://[^\s''"<>),]+)\?[^\s''"<>),]+', '$1?[REDACTED]')
    $sensitiveKeyPattern = '(?:password|passwd|pwd|username|account|displayName|student[_-]?id|studentId|client\.oauth2_token|oauth2[_-]?token|access[_-]?token|id[_-]?token|refresh[_-]?token|token|cookie|set-cookie|authorization|cgAuthorization|ticket|captcha|session[_-]?id|session|photo_path|path|filename|file|lock[_-]?code|lockCode|booking[_-]?id|bookingId|place)'
    $redacted = [regex]::Replace($redacted, '(?i)(?<![A-Za-z0-9_.-])(["'']?' + $sensitiveKeyPattern + '["'']?\s*[:=]\s*)("[^"]*"|''[^'']*''|[^\s,}]+)', '$1[REDACTED]')
    $redacted = [regex]::Replace($redacted, '(?i)(Cookie|Set-Cookie|Authorization|cgAuthorization)\s*:\s*[^\r\n]+', '$1: [REDACTED]')
    $redacted = [regex]::Replace($redacted, '(?i)[A-Z]:[\\/][^\s,}]+', '[REDACTED]')
    $redacted = [regex]::Replace($redacted, '(?i)(/data/|/storage/|/sdcard/)[^\s,}]+', '[REDACTED]')
    $redacted = [regex]::Replace($redacted, '(?is)<!doctype html.*', '[REDACTED]')
    $redacted = [regex]::Replace($redacted, '(?is)<html.*', '[REDACTED]')
    $redacted = [regex]::Replace($redacted, '(?is)<form.*', '[REDACTED]')
    return $redacted
}

function Get-SafeCommandLabel([string[]]$CommandArgs) {
    $parts = @()
    foreach ($arg in $CommandArgs) {
        if ($arg.StartsWith('--')) { break }
        $parts += $arg
    }
    if ($parts.Count -eq 0) { return 'command' }
    return ($parts -join ' ')
}

$LiveSmokeFailures = @()

function Invoke-Ubaa([string[]]$CommandArgs) {
    $safeCommand = "ubaa $(Get-SafeCommandLabel $CommandArgs)"
    Write-Host $safeCommand
    $output = & $CliPath @CommandArgs 2>&1 | Out-String
    $exit = $LASTEXITCODE
    $safe = Redact-Text $output
    if ($exit -ne 0) {
        if (-not [string]::IsNullOrWhiteSpace($safe)) { Write-Host $safe.TrimEnd() }
        $failure = "命令失败，退出码 ${exit}: $safeCommand"
        Write-Host "FAIL：$failure"
        $script:LiveSmokeFailures += $failure
        return
    }
    Write-Host 'OK'
}

function Invoke-UbaaBestEffort([string[]]$CommandArgs) {
    $safeCommand = "ubaa $(Get-SafeCommandLabel $CommandArgs)"
    Write-Host $safeCommand
    $output = & $CliPath @CommandArgs 2>&1 | Out-String
    $exit = $LASTEXITCODE
    $safe = Redact-Text $output
    if ($exit -ne 0) {
        if (-not [string]::IsNullOrWhiteSpace($safe)) { Write-Host $safe.TrimEnd() }
        Write-Host "WARN：best-effort 清理命令失败，退出码 ${exit}: $safeCommand"
        return
    }
    Write-Host 'OK'
}

function Invoke-UbaaJson([string[]]$CommandArgs) {
    $safeCommand = "ubaa $(Get-SafeCommandLabel $CommandArgs)"
    Write-Host $safeCommand
    $output = & $CliPath @CommandArgs 2>&1 | Out-String
    $exit = $LASTEXITCODE
    $safe = Redact-Text $output
    if ($exit -ne 0) {
        if (-not [string]::IsNullOrWhiteSpace($safe)) { Write-Host $safe.TrimEnd() }
        $failure = "命令失败，退出码 ${exit}: $safeCommand"
        Write-Host "FAIL：$failure"
        $script:LiveSmokeFailures += $failure
        return $null
    }
    try {
        $json = $output | ConvertFrom-Json
        Write-Host 'OK'
        return $json
    } catch {
        $failure = "命令返回非 JSON 输出: $safeCommand"
        Write-Host "FAIL：$failure"
        $script:LiveSmokeFailures += $failure
        return $null
    }
}

function Get-IsoDate([datetime]$Date) {
    return $Date.ToString('yyyy-MM-dd', [Globalization.CultureInfo]::InvariantCulture)
}

function Get-LiveWeekRange {
    $today = [datetime]::Today
    $offset = ([int]$today.DayOfWeek + 6) % 7
    $start = $today.AddDays(-$offset)
    $end = $start.AddDays(6)
    return @(
        $(if ($env:UBAANEXT_LIVE_START_DATE) { $env:UBAANEXT_LIVE_START_DATE } else { Get-IsoDate $start }),
        $(if ($env:UBAANEXT_LIVE_END_DATE) { $env:UBAANEXT_LIVE_END_DATE } else { Get-IsoDate $end })
    )
}

function Get-JsonPathValue($Json, [string[]]$Path) {
    $value = $Json
    foreach ($part in $Path) {
        if ($null -eq $value) { return $null }
        $value = $value.$part
    }
    return $value
}

function Get-MutationResultId($Json) {
    $id = Get-JsonPathValue $Json @('data', 'result', 'id')
    if ($null -eq $id) { return '' }
    return [string]$id
}

function Invoke-CloudReversibleWriteSmoke([string]$CloudRootId) {
    if ($env:UBAANEXT_ALLOW_WRITE -ne '1' -or $env:UBAANEXT_CONFIRM_WRITE -ne '1') {
        throw 'Cloud 可逆写 smoke 需要 UBAANEXT_ALLOW_WRITE=1 且 UBAANEXT_CONFIRM_WRITE=1。'
    }

    $stamp = [guid]::NewGuid().ToString('N')
    $dirName = "ubaanext-smoke-$stamp"
    $renamedDirName = "$dirName-renamed"
    $localFile = Join-Path $env:UBAANEXT_APP_DATA_DIR "$dirName.txt"
    Set-Content -LiteralPath $localFile -Value "ubaanext cloud smoke $stamp" -Encoding UTF8

    $dirId = ''
    $shareId = ''
    try {
        $mkdir = Invoke-UbaaJson @('file', 'mkdir', '--mode', $Mode, '--parent-id', $CloudRootId, '--name', $dirName, '--confirm', '--json')
        $dirId = Get-MutationResultId $mkdir
        if ([string]::IsNullOrWhiteSpace($dirId)) { throw 'Cloud mkdir 未返回目录 docid。' }

        Invoke-Ubaa @('file', 'upload', '--mode', $Mode, '--parent-id', $dirId, '--path', $localFile, '--name', 'probe.txt', '--confirm', '--json')
        Invoke-Ubaa @('file', 'list', '--mode', $Mode, '--id', $dirId, '--json')
        Invoke-Ubaa @('file', 'size', '--mode', $Mode, '--id', $dirId, '--json')
        Invoke-Ubaa @('file', 'rename', '--mode', $Mode, '--id', $dirId, '--name', $renamedDirName, '--confirm', '--json')

        $share = Invoke-UbaaJson @('file', 'share-create', '--mode', $Mode, '--id', $dirId, '--is-dir', '--name', "$dirName-share", '--confirm', '--json')
        $shareId = Get-MutationResultId $share
        Invoke-Ubaa @('file', 'share-record', '--mode', $Mode, '--id', $dirId, '--json')
        Invoke-Ubaa @('file', 'download-url', '--mode', $Mode, '--id', $dirId, '--is-dir', '--name', "$dirName.zip", '--json')
    } finally {
        if (-not [string]::IsNullOrWhiteSpace($shareId)) {
            Invoke-Ubaa @('file', 'share-delete', '--mode', $Mode, '--share-id', $shareId, '--confirm', '--json')
        }
        if (-not [string]::IsNullOrWhiteSpace($dirId)) {
            Invoke-Ubaa @('file', 'delete', '--mode', $Mode, '--id', $dirId, '--confirm', '--json')
            if ($env:UBAANEXT_CLOUD_WRITE_CLEAN_RECYCLE -eq '1') {
                Invoke-UbaaBestEffort @('file', 'recycle-delete', '--mode', $Mode, '--id', $dirId, '--confirm', '--json')
            }
        }
        if (Test-Path -LiteralPath $localFile) {
            Remove-Item -LiteralPath $localFile -Force
        }
    }
}

$RunLive = $env:UBAANEXT_LIVE -eq '1'
$RunCloud = $env:UBAANEXT_CLOUD -eq '1'
$RunCloudWrite = $env:UBAANEXT_CLOUD_WRITE -eq '1'
if ($RunCloudWrite) { $RunCloud = $true }

if (-not $RunLive -and -not $RunCloud) {
    Write-Host 'live/cloud smoke 已跳过：需要设置 UBAANEXT_LIVE=1、UBAANEXT_CLOUD=1 或 UBAANEXT_CLOUD_WRITE=1。'
    exit 0
}

if ([string]::IsNullOrWhiteSpace($env:UBAANEXT_USERNAME) -or [string]::IsNullOrWhiteSpace($env:UBAANEXT_PASSWORD)) {
    throw 'live/cloud smoke 需要 UBAANEXT_USERNAME 和 UBAANEXT_PASSWORD。'
}

if (-not (Test-Path $CliPath)) {
    throw "找不到 CLI: $CliPath"
}

if ([string]::IsNullOrWhiteSpace($env:UBAANEXT_APP_DATA_DIR)) {
    $env:UBAANEXT_APP_DATA_DIR = Join-Path $env:TEMP ('ubaanext-live-smoke-' + [guid]::NewGuid().ToString('N'))
}
New-Item -ItemType Directory -Force -Path $env:UBAANEXT_APP_DATA_DIR | Out-Null

Invoke-Ubaa @('login', '--mode', $Mode, '--username', $env:UBAANEXT_USERNAME, '--password', $env:UBAANEXT_PASSWORD, '--json')
Invoke-Ubaa @('whoami', '--json')
Invoke-Ubaa @('term', 'list', '--mode', $Mode, '--json')
if ($RunLive) {
    $liveWeekRange = Get-LiveWeekRange
    Invoke-Ubaa @('live', 'week', '--mode', $Mode, '--start-date', $liveWeekRange[0], '--end-date', $liveWeekRange[1], '--json')
}

if ($RunCloud) {
    $cloudRootKind = $(if ($env:UBAANEXT_CLOUD_ROOT_KIND) { $env:UBAANEXT_CLOUD_ROOT_KIND } else { 'user' })
    Invoke-Ubaa @('file', 'roots', '--mode', $Mode, '--root', $cloudRootKind, '--json')
    $cloudRoot = Invoke-UbaaJson @('file', 'root', '--mode', $Mode, '--json')
    Invoke-Ubaa @('file', 'recycle', '--mode', $Mode, '--json')
    Invoke-Ubaa @('file', 'shares', '--mode', $Mode, '--json')
    if ($null -ne $cloudRoot -and $cloudRoot.ok -eq $true -and $null -ne $cloudRoot.data -and $null -ne $cloudRoot.data.cloudRoot -and -not [string]::IsNullOrWhiteSpace([string]$cloudRoot.data.cloudRoot.id)) {
        $cloudRootId = [string]$cloudRoot.data.cloudRoot.id
        Invoke-Ubaa @('file', 'list', '--mode', $Mode, '--id', $cloudRootId, '--json')
        Invoke-Ubaa @('file', 'size', '--mode', $Mode, '--id', $cloudRootId, '--json')
        if ($RunCloudWrite) {
            Invoke-CloudReversibleWriteSmoke $cloudRootId
        }
    } else {
        $failure = '北航云盘个人根目录未返回可用于 list/size 的 docid'
        Write-Host "FAIL：$failure"
        $script:LiveSmokeFailures += $failure
    }
}

if ($RunLive -and -not [string]::IsNullOrWhiteSpace($env:UBAANEXT_TERM)) {
    Invoke-Ubaa @('week', 'list', '--mode', $Mode, '--term', $env:UBAANEXT_TERM, '--json')
    Invoke-Ubaa @('course', 'week', '--mode', $Mode, '--term', $env:UBAANEXT_TERM, '--week', $(if ($env:UBAANEXT_WEEK) { $env:UBAANEXT_WEEK } else { '1' }), '--json')
    Invoke-Ubaa @('exam', 'list', '--mode', $Mode, '--term', $env:UBAANEXT_TERM, '--json')
    Invoke-Ubaa @('grade', 'list', '--mode', $Mode, '--term', $env:UBAANEXT_TERM, '--json')
}

if ($RunLive) {
    Invoke-Ubaa @('app', 'version', '--mode', $Mode, '--json')
    Invoke-Ubaa @('app', 'announcement', '--mode', $Mode, '--json')
    Invoke-Ubaa @('todo', 'list', '--mode', $Mode, '--json')
    Invoke-Ubaa @('spoc', 'assignments', '--mode', $Mode, '--json')
    Invoke-Ubaa @('judge', 'assignments', '--mode', $Mode, '--json')
    Invoke-Ubaa @('signin', 'today', '--mode', $Mode, '--json')
    Invoke-Ubaa @('ygdk', 'overview', '--mode', $Mode, '--json')
    Invoke-Ubaa @('ygdk', 'records', '--mode', $Mode, '--page', '1', '--size', '20', '--json')
    Invoke-Ubaa @('bykc', 'profile', '--mode', $Mode, '--json')
    Invoke-Ubaa @('bykc', 'courses', '--mode', $Mode, '--json')
    Invoke-Ubaa @('bykc', 'chosen', '--mode', $Mode, '--json')
    Invoke-Ubaa @('bykc', 'stats', '--mode', $Mode, '--json')
    if ($Mode -eq 'direct') {
        Invoke-Ubaa @('cgyy', 'sites', '--mode', 'direct', '--json')
        Invoke-Ubaa @('cgyy', 'orders', '--mode', 'direct', '--page', '1', '--size', '20', '--json')
        Invoke-Ubaa @('cgyy', 'order', 'lock-code', '--mode', 'direct', '--json')
    } else {
        Write-Host 'ubaa cgyy sites/orders/lock-code'
        Write-Host 'SKIP：CGYY 当前仅支持 direct 模式。'
    }
    Invoke-Ubaa @('libbook', 'libraries', '--mode', $Mode, '--json')
    Invoke-Ubaa @('libbook', 'reservations', '--mode', $Mode, '--json')
}

if ($LiveSmokeFailures.Count -gt 0) {
    Write-Host 'live smoke 失败项：'
    foreach ($failure in $LiveSmokeFailures) {
        Write-Host "- $failure"
    }
    exit 4
}

if ($Level -eq 'L1') {
    Write-Host 'live smoke L1 完成。'
    exit 0
}

if ($env:UBAANEXT_ALLOW_WRITE -ne '1') {
    Write-Host 'live smoke 写操作已跳过：需要设置 UBAANEXT_ALLOW_WRITE=1。'
    exit 0
}

Write-Host 'L2/L3 写操作需要手动指定具体命令；本 runner 不自动执行真实写操作。'
if ($Level -eq 'L3' -and $env:UBAANEXT_CONFIRM_WRITE -ne '1') {
    throw 'L3 真实写操作需要 UBAANEXT_CONFIRM_WRITE=1，并由用户手动执行具体命令。'
}
