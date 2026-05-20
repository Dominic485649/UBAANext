param(
    [ValidateSet('L1', 'L2', 'L3')]
    [string]$Level = $(if ($env:UBAANEXT_LIVE_LEVEL) { $env:UBAANEXT_LIVE_LEVEL } else { 'L1' }),
    [string]$CliPath = $(if ($env:UBAANEXT_CLI_PATH) { $env:UBAANEXT_CLI_PATH } else { '.\build\windows-ninja-msvc-debug\apps\cli\ubaa.exe' }),
    [string]$Mode = $(if ($env:UBAANEXT_CONNECTION_MODE) { $env:UBAANEXT_CONNECTION_MODE } else { 'direct' })
)

$ErrorActionPreference = 'Stop'

function Redact-Text([string]$Text) {
    if ($null -eq $Text) { return '' }
    $redacted = $Text
    foreach ($value in @($env:UBAANEXT_PASSWORD, $env:UBAANEXT_TOKEN, $env:UBAANEXT_COOKIE, $env:UBAANEXT_TICKET, $env:UBAANEXT_SESSION)) {
        if (![string]::IsNullOrWhiteSpace($value)) {
            $redacted = $redacted.Replace($value, '[REDACTED]')
        }
    }
    $redacted = [regex]::Replace($redacted, '(?i)(password|passwd|pwd|token|cookie|set-cookie|authorization|ticket|captcha|session[_-]?id|session)\s*[:=]\s*[^\s,}]+', '$1=[REDACTED]')
    return $redacted
}

function Invoke-Ubaa([string[]]$Args) {
    Write-Host "ubaa $($Args -join ' ')"
    $output = & $CliPath @Args 2>&1 | Out-String
    $exit = $LASTEXITCODE
    $safe = Redact-Text $output
    if (-not [string]::IsNullOrWhiteSpace($safe)) { Write-Host $safe.TrimEnd() }
    if ($exit -ne 0) { throw "命令失败，退出码 $exit" }
}

if ($env:UBAANEXT_LIVE -ne '1') {
    Write-Host 'live smoke 已跳过：需要设置 UBAANEXT_LIVE=1。'
    exit 0
}

if ([string]::IsNullOrWhiteSpace($env:UBAANEXT_USERNAME) -or [string]::IsNullOrWhiteSpace($env:UBAANEXT_PASSWORD)) {
    throw 'live smoke 需要 UBAANEXT_USERNAME 和 UBAANEXT_PASSWORD。'
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

if (-not [string]::IsNullOrWhiteSpace($env:UBAANEXT_TERM)) {
    Invoke-Ubaa @('week', 'list', '--mode', $Mode, '--term', $env:UBAANEXT_TERM, '--json')
    Invoke-Ubaa @('course', 'week', '--mode', $Mode, '--term', $env:UBAANEXT_TERM, '--week', $(if ($env:UBAANEXT_WEEK) { $env:UBAANEXT_WEEK } else { '1' }), '--json')
    Invoke-Ubaa @('exam', 'list', '--mode', $Mode, '--term', $env:UBAANEXT_TERM, '--json')
    Invoke-Ubaa @('grade', 'list', '--mode', $Mode, '--term', $env:UBAANEXT_TERM, '--json')
}

Invoke-Ubaa @('app', 'version', '--mode', $Mode, '--json')
Invoke-Ubaa @('app', 'announcement', '--mode', $Mode, '--json')
Invoke-Ubaa @('todo', 'list', '--mode', $Mode, '--json')
Invoke-Ubaa @('spoc', 'assignments', '--mode', $Mode, '--json')
Invoke-Ubaa @('judge', 'assignments', '--mode', $Mode, '--json')
Invoke-Ubaa @('signin', 'today', '--mode', $Mode, '--json')
Invoke-Ubaa @('ygdk', 'overview', '--mode', $Mode, '--json')
Invoke-Ubaa @('bykc', 'profile', '--mode', $Mode, '--json')
Invoke-Ubaa @('bykc', 'courses', '--mode', $Mode, '--json')
Invoke-Ubaa @('cgyy', 'sites', '--mode', 'direct', '--json')
Invoke-Ubaa @('libbook', 'libraries', '--mode', $Mode, '--json')

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
