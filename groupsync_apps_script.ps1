param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Setup', 'Create', 'Collect', 'Fetch', 'Ping', 'Cancel')]
    [string]$Action,
    [string]$InputPath = 'groupsync_event.json',
    [string]$OutputPath = 'groupsync_form.txt',
    [string]$ConfigPath = 'groupsync_apps_script_config.json',
    [string]$FormId,
    [int]$ExpectedResponses = 0
)

$ErrorActionPreference = 'Stop'
$script:DefaultEndpoint = 'https://script.google.com/macros/s/AKfycbwx0iwPcasc0VYE8V-jnueHknai8UhlMRLKP40lx2drn41PzzxNdfpiKTvJkwfuOky5JA/exec'
$script:Utf8NoBom = New-Object System.Text.UTF8Encoding($false)

function Get-Config {
    if (-not (Test-Path -LiteralPath $ConfigPath)) {
        throw 'Apps Script setup is missing. Run GroupSync again to configure it.'
    }
    return (Get-Content -LiteralPath $ConfigPath -Raw | ConvertFrom-Json)
}

function Invoke-AppsScript {
    param(
        [Parameter(Mandatory = $true)]$Config,
        [Parameter(Mandatory = $true)][string]$RequestAction,
        $RequestData = $null,
        [string]$RequestFormId,
        [int]$ResponseCount = 0,
        [bool]$CloseWhenReady = $false
    )

    $body = @{
        apiKey = $Config.apiKey
        action = $RequestAction
    }
    if ($null -ne $RequestData) { $body.event = $RequestData }
    if ($RequestFormId) { $body.formId = $RequestFormId }
    if ($ResponseCount -gt 0) { $body.expectedResponses = $ResponseCount }
    if ($RequestAction -eq 'Collect') { $body.closeWhenReady = $CloseWhenReady }

    $json = ConvertTo-Json -InputObject $body -Depth 100 -Compress
    $result = Invoke-RestMethod -Method Post -Uri $Config.endpoint `
        -ContentType 'application/json; charset=utf-8' -Body $json `
        -MaximumRedirection 10 -TimeoutSec 45
    $cancelConfirmed = $RequestAction -eq 'Cancel' -and
        $null -ne $result -and $result.status -eq 'canceled' -and
        $result.acceptingResponses -eq $false
    if ($null -eq $result -or (-not $result.ok -and -not $cancelConfirmed)) {
        $message = if ($result.error) { $result.error } else { 'The Apps Script returned an invalid response.' }
        throw $message
    }
    return $result
}

function Start-AppsScriptSetup {
    $endpoint = $script:DefaultEndpoint
    Write-Host "Using Apps Script endpoint: $endpoint"
    if ($endpoint -notmatch '^https://script\.google\.com/macros/s/[^/]+/exec$') {
        throw 'Enter the deployed Apps Script URL ending in /exec.'
    }

    $secureKey = Read-Host 'Apps Script API key from the setup execution log' -AsSecureString
    $keyPointer = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secureKey)
    try {
        $apiKey = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($keyPointer)
    }
    finally {
        [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($keyPointer)
    }
    if ([string]::IsNullOrWhiteSpace($apiKey)) { throw 'The API key cannot be empty.' }

    $config = @{ endpoint = $endpoint; apiKey = $apiKey }
    $null = Invoke-AppsScript -Config $config -RequestAction 'Ping'
    $configJson = ConvertTo-Json -InputObject $config -Depth 5
    [System.IO.File]::WriteAllText($ConfigPath, $configJson, $script:Utf8NoBom)
    Write-Host "Connected to Apps Script. Settings saved in $ConfigPath; keep that file private."
}

function New-AvailabilityForm {
    $createdData = Get-Content -LiteralPath $InputPath -Raw | ConvertFrom-Json
    $config = Get-Config
    $form = Invoke-AppsScript -Config $config -RequestAction 'Create' -RequestData $createdData
    if (-not $form.formId -or -not $form.responderUrl) {
        throw 'The Apps Script did not return a form ID and responder URL.'
    }

    [string[]]$outputLines = @($form.formId, $form.responderUrl, [string]$form.sheetUrl)
    [System.IO.File]::WriteAllLines($OutputPath, $outputLines, $script:Utf8NoBom)
    Write-Host "Created form: $($form.responderUrl)"
    if ($form.sheetUrl) { Write-Host "Responses sheet: $($form.sheetUrl)" }
}

function Export-AvailabilityResponses {
    if ($ExpectedResponses -lt 1 -or $ExpectedResponses -gt 50) {
        throw 'ExpectedResponses must be between 1 and 50.'
    }
    if ($FormId -notmatch '^[A-Za-z0-9_-]+$') {
        throw 'The Google Form ID is missing or invalid.'
    }

    $config = Get-Config
    do {
        $result = Invoke-AppsScript -Config $config -RequestAction 'Collect' `
            -RequestFormId $FormId -ResponseCount $ExpectedResponses -CloseWhenReady $true
        if (-not $result.ready) {
            Write-Host "Received $($result.responseCount) of $ExpectedResponses responses. Checking again in 15 seconds; press Ctrl+C to stop."
            Start-Sleep -Seconds 15
        }
    } while (-not $result.ready)

    [System.IO.File]::WriteAllText($OutputPath, [string]$result.tsv, $script:Utf8NoBom)
    Write-Host "Collected $($result.responseCount) responses."
}

function Fetch-AvailabilityResponses {
    if ($ExpectedResponses -lt 1 -or $ExpectedResponses -gt 50) {
        throw 'ExpectedResponses must be between 1 and 50.'
    }
    if ($FormId -notmatch '^[A-Za-z0-9_-]+$') {
        throw 'The Google Form ID is missing or invalid.'
    }

    $config = Get-Config
    $result = Invoke-AppsScript -Config $config -RequestAction 'Collect' `
        -RequestFormId $FormId -ResponseCount $ExpectedResponses -CloseWhenReady $false
    [System.IO.File]::WriteAllText($OutputPath, [string]$result.tsv, $script:Utf8NoBom)
    Write-Host "Fetched $($result.responseCount) of $ExpectedResponses responses."
}

function Test-AppsScriptConnection {
    $config = Get-Config
    $null = Invoke-AppsScript -Config $config -RequestAction 'Ping'
    Write-Host 'Connected to Apps Script.'
}

function Cancel-AvailabilityEvent {
    if ($FormId -notmatch '^[A-Za-z0-9_-]+$') {
        throw 'The Google Form ID is missing or invalid.'
    }

    $config = Get-Config
    $result = Invoke-AppsScript -Config $config -RequestAction 'Cancel' `
        -RequestFormId $FormId
    if ($result.status -ne 'canceled' -or $result.acceptingResponses -ne $false) {
        throw 'The Apps Script did not confirm that the form was canceled and closed.'
    }
    Write-Host 'Google Form canceled. Existing responses were preserved.'
}

try {
    switch ($Action) {
        'Setup' { Start-AppsScriptSetup }
        'Create' { New-AvailabilityForm }
        'Collect' { Export-AvailabilityResponses }
        'Fetch' { Fetch-AvailabilityResponses }
        'Ping' { Test-AppsScriptConnection }
        'Cancel' { Cancel-AvailabilityEvent }
    }
}
catch {
    $details = $_.ErrorDetails.Message
    if ([string]::IsNullOrWhiteSpace($details)) { $details = $_.Exception.Message }
    [Console]::Error.WriteLine("GroupSync Apps Script error: $details")
    exit 1
}