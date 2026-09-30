$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$script:Root = $PSScriptRoot
$script:ConfigPath = Join-Path $script:Root 'groupsync_apps_script_config.json'
$script:EventPath = Join-Path $script:Root 'groupsync_event.json'
$script:ResponsesPath = Join-Path $script:Root 'groupsync_responses.tsv'
$script:DateRows = New-Object System.Collections.ArrayList

function Show-ErrorMessage {
    param([string]$Message)
    [System.Windows.Forms.MessageBox]::Show(
        $Message, 'GroupSync',
        [System.Windows.Forms.MessageBoxButtons]::OK,
        [System.Windows.Forms.MessageBoxIcon]::Error
    ) | Out-Null
}

function Invoke-GroupSyncRequest {
    param(
        [Parameter(Mandatory = $true)]$Config,
        [Parameter(Mandatory = $true)][string]$Action,
        $EventData = $null,
        [string]$FormId,
        [int]$ExpectedResponses = 0
    )

    $body = @{ apiKey = $Config.apiKey; action = $Action }
    if ($null -ne $EventData) { $body.event = $EventData }
    if ($FormId) { $body.formId = $FormId }
    if ($ExpectedResponses -gt 0) { $body.expectedResponses = $ExpectedResponses }
    if ($Action -eq 'Collect') { $body.closeWhenReady = $true }

    $json = ConvertTo-Json -InputObject $body -Depth 100 -Compress
    $response = Invoke-RestMethod -Method Post -Uri $Config.endpoint `
        -ContentType 'application/json; charset=utf-8' -Body $json `
        -MaximumRedirection 10
    if ($null -eq $response -or -not $response.ok) {
        $message = if ($response.error) { $response.error } else { 'Invalid response from Apps Script.' }
        throw $message
    }
    return $response
}

function Format-ClockTime {
    param([int]$Minutes)

    $normalized = $Minutes % 1440
    $hour = [math]::Floor($normalized / 60)
    $minute = $normalized % 60
    $period = if ($hour -lt 12) { 'AM' } else { 'PM' }
    $displayHour = $hour % 12
    if ($displayHour -eq 0) { $displayHour = 12 }
    return ('{0}:{1:00} {2}' -f $displayHour, $minute, $period)
}

function Read-GroupSyncPalette {
    $headerPath = Join-Path $script:Root 'GroupSync_Colors.h'
    if (-not (Test-Path -LiteralPath $headerPath)) {
        throw 'GroupSync_Colors.h was not found beside the GUI script.'
    }

    $palette = @{}
    foreach ($line in Get-Content -LiteralPath $headerPath) {
        if ($line -match '^\s*#define\s+(GS_COLOR_[A-Z_]+)\s+RGB\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)') {
            $palette[$matches[1]] = [System.Drawing.Color]::FromArgb(
                [int]$matches[2], [int]$matches[3], [int]$matches[4]
            )
        }
    }

    $requiredColors = @(
        'GS_COLOR_PRIMARY', 'GS_COLOR_PRIMARY_HOVER', 'GS_COLOR_AVAILABLE',
        'GS_COLOR_AVAILABLE_BG', 'GS_COLOR_WINDOW_BG', 'GS_COLOR_SURFACE',
        'GS_COLOR_TEXT', 'GS_COLOR_TEXT_SECONDARY', 'GS_COLOR_BORDER',
        'GS_COLOR_BUTTON_TEXT'
    )
    $missingColors = @($requiredColors | Where-Object { -not $palette.ContainsKey($_) })
    if ($missingColors.Count -gt 0) {
        throw "Missing palette definitions in GroupSync_Colors.h: $($missingColors -join ', ')"
    }
    return $palette
}

function Get-SuggestionText {
    param($EventData, [string]$Tsv)

    $participants = New-Object System.Collections.ArrayList
    foreach ($line in ($Tsv -split "`r?`n")) {
        if ([string]::IsNullOrWhiteSpace($line)) { continue }
        $fields = $line.Split([char]9)
        if ($fields.Count -lt 2) { continue }
        $statuses = @($fields | Select-Object -Skip 1)
        [void]$participants.Add([pscustomobject]@{
            Name = $fields[0]
            Statuses = $statuses
        })
    }

    if ($participants.Count -eq 0) { return 'No usable participant responses were returned.' }

    $blocksNeeded = [math]::Ceiling($EventData.durationMinutes / 30.0)
    $recommendations = New-Object System.Collections.ArrayList
    $dateOffset = 0
    for ($dateIndex = 0; $dateIndex -lt $EventData.dates.Count; $dateIndex++) {
        $date = $EventData.dates[$dateIndex]
        for ($startBlock = 0; $startBlock + $blocksNeeded -le $date.blockCount; $startBlock++) {
            $availableNames = New-Object System.Collections.ArrayList
            $maybeNames = New-Object System.Collections.ArrayList
            foreach ($participant in $participants) {
                $hasMaybe = $false
                $hasUnavailable = $false
                for ($block = $startBlock; $block -lt $startBlock + $blocksNeeded; $block++) {
                    $statusIndex = $dateOffset + $block
                    $status = if ($statusIndex -lt $participant.Statuses.Count) {
                        $participant.Statuses[$statusIndex]
                    } else { 'U' }
                    if ($status -eq 'U') { $hasUnavailable = $true; break }
                    if ($status -eq 'M') { $hasMaybe = $true }
                }
                if (-not $hasUnavailable) {
                    if ($hasMaybe) { [void]$maybeNames.Add($participant.Name) }
                    else { [void]$availableNames.Add($participant.Name) }
                }
            }
            [void]$recommendations.Add([pscustomobject]@{
                DateIndex = $dateIndex
                StartBlock = $startBlock
                AvailableCount = $availableNames.Count
                MaybeCount = $maybeNames.Count
                AvailableNames = @($availableNames.ToArray())
                MaybeNames = @($maybeNames.ToArray())
            })
        }
        $dateOffset += $date.blockCount
    }

    if ($recommendations.Count -eq 0) {
        return 'No candidate time range is long enough for the meeting.'
    }

    $ranked = @($recommendations | Sort-Object `
        @{ Expression = 'AvailableCount'; Descending = $true },
        @{ Expression = 'MaybeCount'; Descending = $true },
        @{ Expression = 'DateIndex'; Descending = $false },
        @{ Expression = 'StartBlock'; Descending = $false } |
        Select-Object -First 3)

    $builder = New-Object System.Text.StringBuilder
    if ($ranked[0].AvailableCount -lt $participants.Count) {
        [void]$builder.AppendLine('No option works for everyone. Showing the best-attended times.')
        [void]$builder.AppendLine()
    }
    for ($index = 0; $index -lt $ranked.Count; $index++) {
        $option = $ranked[$index]
        $date = $EventData.dates[$option.DateIndex]
        $startMinutes = $date.startMinutes + ($option.StartBlock * 30)
        $endMinutes = $startMinutes + $EventData.durationMinutes
        [void]$builder.AppendLine(('{0}. {1}, {2} - {3} | {4} available, {5} maybe' -f `
            ($index + 1), $date.label, (Format-ClockTime $startMinutes),
            (Format-ClockTime $endMinutes), $option.AvailableCount, $option.MaybeCount))
        [void]$builder.AppendLine(('   Available: {0}' -f $(if ($option.AvailableNames.Count) {
            $option.AvailableNames -join ', '
        } else { 'none' })))
        [void]$builder.AppendLine(('   Maybe: {0}' -f $(if ($option.MaybeNames.Count) {
            $option.MaybeNames -join ', '
        } else { 'none' })))
        [void]$builder.AppendLine()
    }
    return $builder.ToString().TrimEnd()
}

if (-not (Test-Path -LiteralPath $script:ConfigPath)) {
    Show-ErrorMessage 'Apps Script is not configured yet. Run the "GroupSync: Configure Apps Script" task first.'
    exit 1
}

try {
    $script:Config = Get-Content -LiteralPath $script:ConfigPath -Raw | ConvertFrom-Json
    $script:Palette = Read-GroupSyncPalette
} catch {
    Show-ErrorMessage "Could not load GroupSync settings: $($_.Exception.Message)"
    exit 1
}

$form = New-Object System.Windows.Forms.Form
$form.Text = 'GroupSync - Dashboard'
$form.StartPosition = 'CenterScreen'
$form.ClientSize = New-Object System.Drawing.Size(1440, 900)
$form.MinimumSize = New-Object System.Drawing.Size(1320, 760)
$form.Font = New-Object System.Drawing.Font('Segoe UI', 9)
$form.BackColor = $script:Palette['GS_COLOR_WINDOW_BG']

$sidebar = New-Object System.Windows.Forms.Panel
$sidebar.Location = New-Object System.Drawing.Point(0, 0)
$sidebar.Size = New-Object System.Drawing.Size(220, 900)
$sidebar.BackColor = $script:Palette['GS_COLOR_SURFACE']
$sidebar.BorderStyle = 'FixedSingle'
$sidebar.Anchor = 'Top, Bottom, Left'
$form.Controls.Add($sidebar)

$headerPanel = New-Object System.Windows.Forms.Panel
$headerPanel.Location = New-Object System.Drawing.Point(220, 0)
$headerPanel.Size = New-Object System.Drawing.Size(1220, 70)
$headerPanel.BackColor = $script:Palette['GS_COLOR_SURFACE']
$headerPanel.Anchor = 'Top, Left, Right'
$form.Controls.Add($headerPanel)

$detailsCard = New-Object System.Windows.Forms.Panel
$detailsCard.Location = New-Object System.Drawing.Point(250, 158)
$detailsCard.Size = New-Object System.Drawing.Size(720, 182)
$detailsCard.BackColor = $script:Palette['GS_COLOR_SURFACE']
$detailsCard.BorderStyle = 'FixedSingle'
$detailsCard.Anchor = 'Top, Left, Right'
$form.Controls.Add($detailsCard)

$availabilityCard = New-Object System.Windows.Forms.Panel
$availabilityCard.Location = New-Object System.Drawing.Point(250, 358)
$availabilityCard.Size = New-Object System.Drawing.Size(720, 505)
$availabilityCard.BackColor = $script:Palette['GS_COLOR_SURFACE']
$availabilityCard.BorderStyle = 'FixedSingle'
$availabilityCard.Anchor = 'Top, Bottom, Left, Right'
$form.Controls.Add($availabilityCard)

$shareCard = New-Object System.Windows.Forms.Panel
$shareCard.Location = New-Object System.Drawing.Point(1000, 158)
$shareCard.Size = New-Object System.Drawing.Size(416, 300)
$shareCard.BackColor = $script:Palette['GS_COLOR_SURFACE']
$shareCard.BorderStyle = 'FixedSingle'
$shareCard.Anchor = 'Top, Right'
$form.Controls.Add($shareCard)

$resultsCard = New-Object System.Windows.Forms.Panel
$resultsCard.Location = New-Object System.Drawing.Point(1000, 476)
$resultsCard.Size = New-Object System.Drawing.Size(416, 387)
$resultsCard.BackColor = $script:Palette['GS_COLOR_SURFACE']
$resultsCard.BorderStyle = 'FixedSingle'
$resultsCard.Anchor = 'Top, Bottom, Right'
$form.Controls.Add($resultsCard)

$brand = New-Object System.Windows.Forms.Label
$brand.Text = 'GroupSync'
$brand.Location = New-Object System.Drawing.Point(64, 24)
$brand.Size = New-Object System.Drawing.Size(130, 30)
$brand.Font = New-Object System.Drawing.Font('Segoe UI Semibold', 16)
$brand.ForeColor = $script:Palette['GS_COLOR_TEXT']
$sidebar.Controls.Add($brand)

$navHeading = New-Object System.Windows.Forms.Label
$navHeading.Text = 'WORKSPACE'
$navHeading.Location = New-Object System.Drawing.Point(20, 94)
$navHeading.Size = New-Object System.Drawing.Size(170, 20)
$navHeading.Font = New-Object System.Drawing.Font('Segoe UI', 8, [System.Drawing.FontStyle]::Bold)
$navHeading.ForeColor = $script:Palette['GS_COLOR_TEXT_SECONDARY']
$sidebar.Controls.Add($navHeading)

$navItem = New-Object System.Windows.Forms.Label
$navItem.Text = 'Dashboard'
$navItem.Location = New-Object System.Drawing.Point(10, 122)
$navItem.Size = New-Object System.Drawing.Size(190, 40)
$navItem.Padding = New-Object System.Windows.Forms.Padding(14, 11, 0, 0)
$navItem.BackColor = [System.Drawing.Color]::FromArgb(234, 236, 254)
$navItem.ForeColor = $script:Palette['GS_COLOR_PRIMARY']
$navItem.Font = New-Object System.Drawing.Font('Segoe UI Semibold', 9)
$sidebar.Controls.Add($navItem)

$sidebarStatus = New-Object System.Windows.Forms.Label
$sidebarStatus.Text = "APPS SCRIPT`r`nConfigured"
$sidebarStatus.Location = New-Object System.Drawing.Point(20, 790)
$sidebarStatus.Size = New-Object System.Drawing.Size(180, 48)
$sidebarStatus.Anchor = 'Bottom, Left'
$sidebarStatus.ForeColor = $script:Palette['GS_COLOR_TEXT_SECONDARY']
$sidebarStatus.Font = New-Object System.Drawing.Font('Segoe UI', 9)
$sidebar.Controls.Add($sidebarStatus)

$dashboardTitle = New-Object System.Windows.Forms.Label
$dashboardTitle.Text = 'Dashboard'
$dashboardTitle.Location = New-Object System.Drawing.Point(250, 21)
$dashboardTitle.Size = New-Object System.Drawing.Size(260, 32)
$dashboardTitle.Font = New-Object System.Drawing.Font('Segoe UI Semibold', 16)
$dashboardTitle.ForeColor = $script:Palette['GS_COLOR_TEXT']
$form.Controls.Add($dashboardTitle)

$configuredLabel = New-Object System.Windows.Forms.Label
$configuredLabel.Text = 'Apps Script configured'
$configuredLabel.Location = New-Object System.Drawing.Point(1200, 25)
$configuredLabel.Size = New-Object System.Drawing.Size(205, 24)
$configuredLabel.TextAlign = 'MiddleRight'
$configuredLabel.ForeColor = $script:Palette['GS_COLOR_AVAILABLE']
$configuredLabel.Anchor = 'Top, Right'
$form.Controls.Add($configuredLabel)

$eyebrow = New-Object System.Windows.Forms.Label
$eyebrow.Text = 'GROUP AVAILABILITY'
$eyebrow.Location = New-Object System.Drawing.Point(250, 91)
$eyebrow.Size = New-Object System.Drawing.Size(300, 18)
$eyebrow.Font = New-Object System.Drawing.Font('Segoe UI', 8, [System.Drawing.FontStyle]::Bold)
$eyebrow.ForeColor = $script:Palette['GS_COLOR_TEXT_SECONDARY']
$form.Controls.Add($eyebrow)

$title = New-Object System.Windows.Forms.Label
$title.Text = 'Plan a group event'
$title.Location = New-Object System.Drawing.Point(250, 109)
$title.Size = New-Object System.Drawing.Size(600, 36)
$title.Font = New-Object System.Drawing.Font('Segoe UI Semibold', 18)
$title.ForeColor = $script:Palette['GS_COLOR_TEXT']
$form.Controls.Add($title)

$logoPath = Join-Path $script:Root 'GroupSyncLogo.png'
if (-not (Test-Path -LiteralPath $logoPath)) {
    Show-ErrorMessage 'GroupSyncLogo.png was not found beside the GUI script.'
    exit 1
}
$script:LogoImage = [System.Drawing.Image]::FromFile($logoPath)
$logo = New-Object System.Windows.Forms.PictureBox
$logo.Image = $script:LogoImage
$logo.SizeMode = 'Zoom'
$logo.Location = New-Object System.Drawing.Point(20, 22)
$logo.Size = New-Object System.Drawing.Size(34, 34)
$logo.BackColor = $script:Palette['GS_COLOR_SURFACE']
$logo.AccessibleName = 'GroupSync logo'
$sidebar.Controls.Add($logo)
$logo.BringToFront()

$eventNameLabel = New-Object System.Windows.Forms.Label
$eventNameLabel.Text = 'Event name'
$eventNameLabel.Location = New-Object System.Drawing.Point(270, 174)
$eventNameLabel.Size = New-Object System.Drawing.Size(180, 22)
$eventNameLabel.Font = New-Object System.Drawing.Font('Segoe UI Semibold', 9)
$eventNameLabel.ForeColor = $script:Palette['GS_COLOR_TEXT']
$form.Controls.Add($eventNameLabel)

$eventNameBox = New-Object System.Windows.Forms.TextBox
$eventNameBox.Location = New-Object System.Drawing.Point(270, 197)
$eventNameBox.Size = New-Object System.Drawing.Size(680, 28)
$eventNameBox.Font = New-Object System.Drawing.Font('Segoe UI', 10)
$eventNameBox.Anchor = 'Top, Left, Right'
$form.Controls.Add($eventNameBox)

$durationLabel = New-Object System.Windows.Forms.Label
$durationLabel.Text = 'Meeting length (minutes)'
$durationLabel.Location = New-Object System.Drawing.Point(270, 238)
$durationLabel.Size = New-Object System.Drawing.Size(180, 22)
$durationLabel.Font = New-Object System.Drawing.Font('Segoe UI Semibold', 9)
$durationLabel.ForeColor = $script:Palette['GS_COLOR_TEXT']
$form.Controls.Add($durationLabel)

$durationInput = New-Object System.Windows.Forms.NumericUpDown
$durationInput.Location = New-Object System.Drawing.Point(270, 261)
$durationInput.Size = New-Object System.Drawing.Size(130, 28)
$durationInput.Minimum = 1
$durationInput.Maximum = 1440
$durationInput.Increment = 30
$durationInput.Value = 60
$durationInput.Font = New-Object System.Drawing.Font('Segoe UI', 10)
$form.Controls.Add($durationInput)

$responseCountLabel = New-Object System.Windows.Forms.Label
$responseCountLabel.Text = 'Expected responses'
$responseCountLabel.Location = New-Object System.Drawing.Point(440, 238)
$responseCountLabel.Size = New-Object System.Drawing.Size(170, 22)
$responseCountLabel.Font = New-Object System.Drawing.Font('Segoe UI Semibold', 9)
$responseCountLabel.ForeColor = $script:Palette['GS_COLOR_TEXT']
$form.Controls.Add($responseCountLabel)

$responseCountInput = New-Object System.Windows.Forms.NumericUpDown
$responseCountInput.Location = New-Object System.Drawing.Point(440, 261)
$responseCountInput.Size = New-Object System.Drawing.Size(100, 28)
$responseCountInput.Minimum = 1
$responseCountInput.Maximum = 50
$responseCountInput.Value = 3
$responseCountInput.Font = New-Object System.Drawing.Font('Segoe UI', 10)
$form.Controls.Add($responseCountInput)

$datesLabel = New-Object System.Windows.Forms.Label
$datesLabel.Text = 'Group Availability'
$datesLabel.Location = New-Object System.Drawing.Point(270, 378)
$datesLabel.Size = New-Object System.Drawing.Size(360, 24)
$datesLabel.Font = New-Object System.Drawing.Font('Segoe UI Semibold', 10)
$datesLabel.ForeColor = $script:Palette['GS_COLOR_TEXT']
$form.Controls.Add($datesLabel)

$datesHint = New-Object System.Windows.Forms.Label
$datesHint.Text = 'Choose the dates and time ranges your group can consider.'
$datesHint.Location = New-Object System.Drawing.Point(270, 402)
$datesHint.Size = New-Object System.Drawing.Size(500, 22)
$datesHint.ForeColor = $script:Palette['GS_COLOR_TEXT_SECONDARY']
$form.Controls.Add($datesHint)

$addDateButton = New-Object System.Windows.Forms.Button
$addDateButton.Text = '+  Add date'
$addDateButton.Location = New-Object System.Drawing.Point(830, 373)
$addDateButton.Size = New-Object System.Drawing.Size(112, 32)
$addDateButton.FlatStyle = 'Flat'
$addDateButton.FlatAppearance.BorderColor = $script:Palette['GS_COLOR_BORDER']
$addDateButton.FlatAppearance.BorderSize = 1
$addDateButton.BackColor = $script:Palette['GS_COLOR_SURFACE']
$addDateButton.ForeColor = $script:Palette['GS_COLOR_PRIMARY']
$addDateButton.FlatAppearance.MouseOverBackColor = $script:Palette['GS_COLOR_AVAILABLE_BG']
$addDateButton.Cursor = [System.Windows.Forms.Cursors]::Hand
$form.Controls.Add($addDateButton)

$script:DatesPanel = New-Object System.Windows.Forms.FlowLayoutPanel
$script:DatesPanel.Location = New-Object System.Drawing.Point(266, 432)
$script:DatesPanel.Size = New-Object System.Drawing.Size(688, 360)
$script:DatesPanel.AutoScroll = $true
$script:DatesPanel.FlowDirection = 'TopDown'
$script:DatesPanel.WrapContents = $false
$script:DatesPanel.Padding = New-Object System.Windows.Forms.Padding(7)
$script:DatesPanel.BackColor = $script:Palette['GS_COLOR_WINDOW_BG']
$script:DatesPanel.BorderStyle = 'FixedSingle'
$script:DatesPanel.Anchor = 'Top, Bottom, Left, Right'
$form.Controls.Add($script:DatesPanel)

$statusLabel = New-Object System.Windows.Forms.Label
$statusLabel.Text = 'Create a form to share this event with your group.'
$statusLabel.Location = New-Object System.Drawing.Point(1020, 281)
$statusLabel.Size = New-Object System.Drawing.Size(376, 34)
$statusLabel.ForeColor = $script:Palette['GS_COLOR_TEXT_SECONDARY']
$statusLabel.Anchor = 'Top, Right'
$form.Controls.Add($statusLabel)

$shareHeading = New-Object System.Windows.Forms.Label
$shareHeading.Text = 'Share Event'
$shareHeading.Location = New-Object System.Drawing.Point(1020, 178)
$shareHeading.Size = New-Object System.Drawing.Size(340, 26)
$shareHeading.Font = New-Object System.Drawing.Font('Segoe UI Semibold', 13)
$shareHeading.ForeColor = $script:Palette['GS_COLOR_TEXT']
$shareHeading.Anchor = 'Top, Right'
$form.Controls.Add($shareHeading)

$shareHint = New-Object System.Windows.Forms.Label
$shareHint.Text = 'Create a link to collect availability responses.'
$shareHint.Location = New-Object System.Drawing.Point(1020, 207)
$shareHint.Size = New-Object System.Drawing.Size(370, 38)
$shareHint.ForeColor = $script:Palette['GS_COLOR_TEXT_SECONDARY']
$shareHint.Anchor = 'Top, Right'
$form.Controls.Add($shareHint)

$createButton = New-Object System.Windows.Forms.Button
$createButton.Text = 'Create availability form'
$createButton.Location = New-Object System.Drawing.Point(1020, 244)
$createButton.Size = New-Object System.Drawing.Size(205, 36)
$createButton.BackColor = $script:Palette['GS_COLOR_PRIMARY']
$createButton.ForeColor = $script:Palette['GS_COLOR_BUTTON_TEXT']
$createButton.FlatStyle = 'Flat'
$createButton.FlatAppearance.BorderSize = 0
$createButton.FlatAppearance.MouseOverBackColor = $script:Palette['GS_COLOR_PRIMARY_HOVER']
$createButton.Cursor = [System.Windows.Forms.Cursors]::Hand
$createButton.Anchor = 'Top, Right'
$form.Controls.Add($createButton)

$script:FormLink = New-Object System.Windows.Forms.LinkLabel
$script:FormLink.Text = ''
$script:FormLink.Location = New-Object System.Drawing.Point(1020, 322)
$script:FormLink.Size = New-Object System.Drawing.Size(272, 30)
$script:FormLink.Font = New-Object System.Drawing.Font('Segoe UI', 9)
$script:FormLink.LinkColor = $script:Palette['GS_COLOR_PRIMARY']
$script:FormLink.ActiveLinkColor = $script:Palette['GS_COLOR_PRIMARY_HOVER']
$script:FormLink.AutoEllipsis = $true
$script:FormLink.Anchor = 'Top, Right'
$script:FormLink.Visible = $false
$script:FormLink.add_LinkClicked({
    if ($script:FormLink.Text) { Start-Process $script:FormLink.Text }
})
$form.Controls.Add($script:FormLink)

$copyButton = New-Object System.Windows.Forms.Button
$copyButton.Text = 'Copy link'
$copyButton.Location = New-Object System.Drawing.Point(1302, 318)
$copyButton.Size = New-Object System.Drawing.Size(94, 32)
$copyButton.Enabled = $false
$copyButton.FlatStyle = 'Flat'
$copyButton.FlatAppearance.BorderColor = $script:Palette['GS_COLOR_BORDER']
$copyButton.BackColor = $script:Palette['GS_COLOR_SURFACE']
$copyButton.ForeColor = $script:Palette['GS_COLOR_PRIMARY']
$copyButton.FlatAppearance.MouseOverBackColor = $script:Palette['GS_COLOR_AVAILABLE_BG']
$copyButton.Cursor = [System.Windows.Forms.Cursors]::Hand
$copyButton.Anchor = 'Top, Right'
$copyButton.Add_Click({
    if ($script:FormLink.Text) { [System.Windows.Forms.Clipboard]::SetText($script:FormLink.Text) }
})
$form.Controls.Add($copyButton)

$collectButton = New-Object System.Windows.Forms.Button
$collectButton.Text = 'Wait for responses'
$collectButton.Location = New-Object System.Drawing.Point(1020, 526)
$collectButton.Size = New-Object System.Drawing.Size(190, 34)
$collectButton.Enabled = $false
$collectButton.FlatStyle = 'Flat'
$collectButton.FlatAppearance.BorderColor = $script:Palette['GS_COLOR_BORDER']
$collectButton.BackColor = $script:Palette['GS_COLOR_SURFACE']
$collectButton.ForeColor = $script:Palette['GS_COLOR_PRIMARY']
$collectButton.FlatAppearance.MouseOverBackColor = $script:Palette['GS_COLOR_AVAILABLE_BG']
$collectButton.Cursor = [System.Windows.Forms.Cursors]::Hand
$form.Controls.Add($collectButton)

$cancelButton = New-Object System.Windows.Forms.Button
$cancelButton.Text = 'Stop waiting'
$cancelButton.Location = New-Object System.Drawing.Point(1218, 526)
$cancelButton.Size = New-Object System.Drawing.Size(112, 34)
$cancelButton.Enabled = $false
$cancelButton.FlatStyle = 'Flat'
$cancelButton.FlatAppearance.BorderColor = $script:Palette['GS_COLOR_BORDER']
$cancelButton.BackColor = $script:Palette['GS_COLOR_SURFACE']
$cancelButton.Cursor = [System.Windows.Forms.Cursors]::Hand
$cancelButton.Anchor = 'Top, Right'
$form.Controls.Add($cancelButton)

$resultsLabel = New-Object System.Windows.Forms.Label
$resultsLabel.Text = 'Best Meeting Times'
$resultsLabel.Location = New-Object System.Drawing.Point(1020, 496)
$resultsLabel.Size = New-Object System.Drawing.Size(340, 24)
$resultsLabel.Font = New-Object System.Drawing.Font('Segoe UI Semibold', 13)
$resultsLabel.ForeColor = $script:Palette['GS_COLOR_TEXT_SECONDARY']
$form.Controls.Add($resultsLabel)

$resultsBox = New-Object System.Windows.Forms.RichTextBox
$resultsBox.Location = New-Object System.Drawing.Point(1018, 570)
$resultsBox.Size = New-Object System.Drawing.Size(380, 270)
$resultsBox.ReadOnly = $true
$resultsBox.Font = New-Object System.Drawing.Font('Segoe UI', 9)
$resultsBox.BackColor = $script:Palette['GS_COLOR_SURFACE']
$resultsBox.ForeColor = $script:Palette['GS_COLOR_TEXT']
$resultsBox.BorderStyle = 'None'
$resultsBox.Anchor = 'Top, Bottom, Right'
$form.Controls.Add($resultsBox)

function Add-DateRow {
    if ($script:DateRows.Count -ge 14) { return }

    $rowPanel = New-Object System.Windows.Forms.Panel
    $rowPanel.Size = New-Object System.Drawing.Size(
        [math]::Max(460, $script:DatesPanel.ClientSize.Width - 34), 42
    )
    $rowPanel.Margin = New-Object System.Windows.Forms.Padding(4, 3, 4, 3)
    $rowPanel.BackColor = $script:Palette['GS_COLOR_SURFACE']
    $rowPanel.BorderStyle = 'FixedSingle'

    $datePicker = New-Object System.Windows.Forms.DateTimePicker
    $datePicker.Format = 'Custom'
    $datePicker.CustomFormat = 'yyyy-MM-dd'
    $datePicker.Location = New-Object System.Drawing.Point(8, 7)
    $datePicker.Size = New-Object System.Drawing.Size(140, 25)
    $rowPanel.Controls.Add($datePicker)

    $startLabel = New-Object System.Windows.Forms.Label
    $startLabel.Text = 'Start'
    $startLabel.Location = New-Object System.Drawing.Point(160, 11)
    $startLabel.Size = New-Object System.Drawing.Size(40, 20)
    $startLabel.ForeColor = $script:Palette['GS_COLOR_TEXT_SECONDARY']
    $rowPanel.Controls.Add($startLabel)

    $startPicker = New-Object System.Windows.Forms.DateTimePicker
    $startPicker.Format = 'Custom'
    $startPicker.CustomFormat = 'HH:mm'
    $startPicker.ShowUpDown = $true
    $startPicker.Value = [datetime]::Today.AddHours(9)
    $startPicker.Location = New-Object System.Drawing.Point(202, 7)
    $startPicker.Size = New-Object System.Drawing.Size(75, 25)
    $rowPanel.Controls.Add($startPicker)

    $endLabel = New-Object System.Windows.Forms.Label
    $endLabel.Text = 'End'
    $endLabel.Location = New-Object System.Drawing.Point(294, 11)
    $endLabel.Size = New-Object System.Drawing.Size(35, 20)
    $endLabel.ForeColor = $script:Palette['GS_COLOR_TEXT_SECONDARY']
    $rowPanel.Controls.Add($endLabel)

    $endPicker = New-Object System.Windows.Forms.DateTimePicker
    $endPicker.Format = 'Custom'
    $endPicker.CustomFormat = 'HH:mm'
    $endPicker.ShowUpDown = $true
    $endPicker.Value = [datetime]::Today.AddHours(11)
    $endPicker.Location = New-Object System.Drawing.Point(332, 7)
    $endPicker.Size = New-Object System.Drawing.Size(75, 25)
    $rowPanel.Controls.Add($endPicker)

    $removeButton = New-Object System.Windows.Forms.Button
    $removeButton.Text = 'Remove'
    $removeButtonX = [math]::Max(420, $rowPanel.Width - 82)
    $removeButton.Location = [System.Drawing.Point]::new($removeButtonX, 6)
    $removeButton.Size = New-Object System.Drawing.Size(74, 29)
    $removeButton.FlatStyle = 'Flat'
    $removeButton.FlatAppearance.BorderColor = $script:Palette['GS_COLOR_BORDER']
    $removeButton.ForeColor = $script:Palette['GS_COLOR_TEXT_SECONDARY']
    $removeButton.Cursor = [System.Windows.Forms.Cursors]::Hand
    $removeButton.Anchor = 'Top, Right'
    $row = [pscustomobject]@{
        Panel = $rowPanel
        DatePicker = $datePicker
        StartPicker = $startPicker
        EndPicker = $endPicker
    }
    $removeButton.Tag = $row
    $removeButton.Add_Click({
        $target = $this.Tag
        $script:DatesPanel.Controls.Remove($target.Panel)
        [void]$script:DateRows.Remove($target)
        $addDateButton.Enabled = $script:DateRows.Count -lt 14
    }.GetNewClosure())
    $rowPanel.Controls.Add($removeButton)

    [void]$script:DateRows.Add($row)
    $script:DatesPanel.Controls.Add($rowPanel)
    $addDateButton.Enabled = $script:DateRows.Count -lt 14
}

$addDateButton.Add_Click({ Add-DateRow })
Add-DateRow

$script:CreateWorker = New-Object System.ComponentModel.BackgroundWorker
$script:CreateWorker.add_DoWork({
    param($sender, $eventArgs)
    $eventData = $eventArgs.Argument
    [System.IO.File]::WriteAllText($script:EventPath,
        (ConvertTo-Json -InputObject $eventData -Depth 20),
        (New-Object System.Text.UTF8Encoding($false)))
    $eventArgs.Result = Invoke-GroupSyncRequest -Config $script:Config `
        -Action 'Create' -EventData $eventData
})
$script:CreateWorker.add_RunWorkerCompleted({
    param($sender, $eventArgs)
    $createButton.Enabled = $true
    if ($eventArgs.Error) {
        $statusLabel.Text = 'Form creation failed.'
        Show-ErrorMessage $eventArgs.Error.Message
        return
    }
    if (-not $eventArgs.Result.formId -or -not $eventArgs.Result.responderUrl) {
        $statusLabel.Text = 'Form creation failed.'
        Show-ErrorMessage 'Apps Script did not return a form ID and responder link.'
        return
    }
    $script:CreatedFormId = [string]$eventArgs.Result.formId
    $script:CreatedEvent = $script:PendingEvent
    $script:FormLink.Text = [string]$eventArgs.Result.responderUrl
    $script:FormLink.Visible = $true
    $copyButton.Enabled = $true
    $collectButton.Enabled = $true
    $statusLabel.Text = 'Form ready. Share the link, then wait for responses.'
    $resultsBox.Text = 'Form created. Participants can submit their availability using the link above.'
})

$script:CollectWorker = New-Object System.ComponentModel.BackgroundWorker
$script:CollectWorker.WorkerReportsProgress = $true
$script:CollectWorker.WorkerSupportsCancellation = $true
$script:CollectWorker.add_DoWork({
    param($sender, $eventArgs)
    $request = $eventArgs.Argument
    while ($true) {
        if ($sender.CancellationPending) {
            $eventArgs.Cancel = $true
            return
        }
        $response = Invoke-GroupSyncRequest -Config $script:Config `
            -Action 'Collect' -FormId $request.FormId `
            -ExpectedResponses $request.ExpectedResponses
        if ($response.ready) {
            [System.IO.File]::WriteAllText($script:ResponsesPath,
                [string]$response.tsv, (New-Object System.Text.UTF8Encoding($false)))
            $eventArgs.Result = [pscustomobject]@{
                Tsv = [string]$response.tsv
                EventData = $request.EventData
                ResponseCount = [int]$response.responseCount
            }
            return
        }
        $sender.ReportProgress(0, "Received $($response.responseCount) of $($request.ExpectedResponses) responses. Checking again in 15 seconds.")
        for ($second = 0; $second -lt 15; $second++) {
            if ($sender.CancellationPending) {
                $eventArgs.Cancel = $true
                return
            }
            Start-Sleep -Seconds 1
        }
    }
})
$script:CollectWorker.add_ProgressChanged({
    param($sender, $eventArgs)
    $statusLabel.Text = [string]$eventArgs.UserState
})
$script:CollectWorker.add_RunWorkerCompleted({
    param($sender, $eventArgs)
    $collectButton.Enabled = $true
    $cancelButton.Enabled = $false
    if ($eventArgs.Cancelled) {
        $statusLabel.Text = 'Stopped waiting. The Form remains open.'
        return
    }
    if ($eventArgs.Error) {
        $statusLabel.Text = 'Could not collect responses.'
        Show-ErrorMessage $eventArgs.Error.Message
        return
    }
    $statusLabel.Text = "Collected $($eventArgs.Result.ResponseCount) responses."
    $resultsBox.Text = Get-SuggestionText -EventData $eventArgs.Result.EventData `
        -Tsv $eventArgs.Result.Tsv
})

$createButton.Add_Click({
    $name = $eventNameBox.Text.Trim()
    if ([string]::IsNullOrWhiteSpace($name) -or $name.Length -gt 99) {
        Show-ErrorMessage 'Enter an event name of 1 to 99 characters.'
        return
    }
    if ($script:DateRows.Count -lt 1) {
        Show-ErrorMessage 'Add at least one possible date.'
        return
    }

    $dateData = New-Object System.Collections.ArrayList
    foreach ($row in $script:DateRows) {
        $start = ($row.StartPicker.Value.Hour * 60) + $row.StartPicker.Value.Minute
        $end = ($row.EndPicker.Value.Hour * 60) + $row.EndPicker.Value.Minute
        if (($row.StartPicker.Value.Minute % 30) -ne 0 -or
            ($row.EndPicker.Value.Minute % 30) -ne 0 -or $end -le $start) {
            Show-ErrorMessage 'Each time range must end after it starts and use 30-minute boundaries.'
            return
        }
        $label = $row.DatePicker.Value.ToString('yyyy-MM-dd')
        [void]$dateData.Add([pscustomobject]@{
            label = $label
            startMinutes = $start
            endMinutes = $end
            blockCount = [int](($end - $start) / 30)
        })
    }

    $script:PendingEvent = [pscustomobject]@{
        eventName = $name
        durationMinutes = [int]$durationInput.Value
        dates = @($dateData.ToArray())
    }
    $script:PendingExpectedResponses = [int]$responseCountInput.Value
    $createButton.Enabled = $false
    $statusLabel.Text = 'Creating the Google Form...'
    $resultsBox.Clear()
    $script:CreateWorker.RunWorkerAsync($script:PendingEvent)
})

$collectButton.Add_Click({
    $collectButton.Enabled = $false
    $cancelButton.Enabled = $true
    $statusLabel.Text = 'Checking for participant submissions...'
    $request = [pscustomobject]@{
        FormId = $script:CreatedFormId
        EventData = $script:CreatedEvent
        ExpectedResponses = $script:PendingExpectedResponses
    }
    $script:CollectWorker.RunWorkerAsync($request)
})

$cancelButton.Add_Click({
    if ($script:CollectWorker.IsBusy) { $script:CollectWorker.CancelAsync() }
    $cancelButton.Enabled = $false
})

[void]$form.ShowDialog()