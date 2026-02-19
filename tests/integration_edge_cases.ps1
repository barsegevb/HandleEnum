Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:Passed = 0
$script:Failed = 0
$script:Skipped = 0

function Resolve-HandleEnumExe {
    $candidate = Join-Path $PSScriptRoot "..\build\HandleEnum.exe"
    $fullPath = [System.IO.Path]::GetFullPath($candidate)
    if (-not (Test-Path $fullPath)) {
        throw "HandleEnum executable not found at: $fullPath"
    }
    return $fullPath
}

function Test-Result {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][bool]$Condition,
        [string]$Details = ""
    )

    if ($Condition) {
        $script:Passed++
        Write-Host "[PASS] $Name"
    } else {
        $script:Failed++
        Write-Host "[FAIL] $Name"
        if ($Details) {
            Write-Host "       $Details"
        }
    }
}

function Test-Skip {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [string]$Reason = ""
    )
    $script:Skipped++
    Write-Host "[SKIP] $Name"
    if ($Reason) {
        Write-Host "       $Reason"
    }
}

function Invoke-HandleEnum {
    param(
        [Parameter(Mandatory = $true)][string]$ExePath,
        [Parameter(Mandatory = $true)][string[]]$Args,
        [int]$TimeoutSeconds = 45
    )

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $ExePath
    $escapedArgs = @()
    foreach ($arg in $Args) {
        $escaped = $arg.Replace('"', '""')
        if ($escaped -match '\s|"') {
            $escapedArgs += '"' + $escaped + '"'
        } else {
            $escapedArgs += $escaped
        }
    }
    $psi.Arguments = ($escapedArgs -join ' ')
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $psi

    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    [void]$process.Start()

    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        try { $process.Kill($true) } catch {}
        return [pscustomobject]@{
            TimedOut = $true
            ExitCode = -1
            Output = ""
            Error = "Timed out after $TimeoutSeconds seconds"
            DurationMs = $stopwatch.ElapsedMilliseconds
        }
    }

    $stdout = $process.StandardOutput.ReadToEnd()
    $stderr = $process.StandardError.ReadToEnd()
    $stopwatch.Stop()

    return [pscustomobject]@{
        TimedOut = $false
        ExitCode = $process.ExitCode
        Output = $stdout
        Error = $stderr
        DurationMs = $stopwatch.ElapsedMilliseconds
    }
}

function Get-MatchingCount {
    param([Parameter(Mandatory = $true)][string]$Output)
    $match = [regex]::Match($Output, 'Matching handles:\s*(\d+)')
    if (-not $match.Success) {
        return $null
    }
    return [int]$match.Groups[1].Value
}

function Get-DataRows {
    param([Parameter(Mandatory = $true)][string]$Output)

    $rows = @()
    $lines = $Output -split "`r?`n"
    foreach ($line in $lines) {
        $m = [regex]::Match($line, '^\s*(\d+)\s+(.+?)\s+0x([0-9A-F]+)\s+([A-Za-z][A-Za-z0-9_]*)\s+(.+)$')
        if ($m.Success) {
            $rows += [pscustomobject]@{
                Pid = [int]$m.Groups[1].Value
                Process = $m.Groups[2].Value.Trim()
                HandleHex = $m.Groups[3].Value
                Type = $m.Groups[4].Value.Trim()
                Name = $m.Groups[5].Value.Trim()
            }
        }
    }
    return ,$rows
}

function Find-PidWithNAName {
    param([Parameter(Mandatory = $true)][string]$ExePath)

    $candidatePids = Get-Process |
        Sort-Object -Property Id |
        Select-Object -First 40 -ExpandProperty Id

    foreach ($candidateProcessId in $candidatePids) {
        $result = Invoke-HandleEnum -ExePath $ExePath -Args @('--pid', "$candidateProcessId", '--sort', 'pid') -TimeoutSeconds 20
        if ($result.TimedOut -or $result.ExitCode -ne 0) {
            continue
        }

        if ($result.Output -match '\bN/A\b') {
            return [int]$candidateProcessId
        }
    }

    return $null
}

function Find-EventNameSubstringForPid {
    param(
        [Parameter(Mandatory = $true)][string]$ExePath,
        [Parameter(Mandatory = $true)][int]$TargetPid
    )

    $result = Invoke-HandleEnum -ExePath $ExePath -Args @('--pid', "$TargetPid", '--type', 'Event')
    if ($result.TimedOut -or $result.ExitCode -ne 0) {
        return $null
    }

    $rows = Get-DataRows -Output $result.Output
    foreach ($row in $rows) {
        if ($row.Type -ieq 'Event' -and
            $row.Name -ne 'N/A' -and
            $row.Name -notlike 'Locked*' -and
            $row.Name.Length -ge 4) {
            return $row.Name.Substring(0, [Math]::Min(8, $row.Name.Length))
        }
    }
    return $null
}

function Find-PidWithOnlyPlaceholderNames {
    param([Parameter(Mandatory = $true)][string]$ExePath)

    $sampledPids = Get-Process |
        Sort-Object -Property Id |
        Select-Object -First 12 -ExpandProperty Id

    $candidatePids = @($PID) + @($sampledPids | Where-Object { $_ -ne $PID })

    foreach ($candidateProcessId in $candidatePids) {
        $result = Invoke-HandleEnum -ExePath $ExePath -Args @('--pid', "$candidateProcessId", '--sort', 'name') -TimeoutSeconds 6
        if ($result.TimedOut -or $result.ExitCode -ne 0) {
            continue
        }

        $rows = Get-DataRows -Output $result.Output
        if ($rows.Count -eq 0) {
            continue
        }

        $allPlaceholders = $true
        foreach ($row in $rows) {
            $isPlaceholder = ($row.Name -eq 'N/A') -or ($row.Name -eq 'Locked (Anti-Deadlock)')
            if (-not $isPlaceholder) {
                $allPlaceholders = $false
                break
            }
        }

        if ($allPlaceholders) {
            return [int]$candidateProcessId
        }
    }

    return $null
}

Write-Host "== HandleEnum Integration Edge-Case Test Suite =="
$exe = Resolve-HandleEnumExe
Write-Host "Using executable: $exe"

Write-Host "`n[1] Empty Results Handling"
$t1 = Invoke-HandleEnum -ExePath $exe -Args @('--pid', '999999', '--count')
Test-Result -Name 'Non-existent PID returns EXIT_SUCCESS' -Condition ((-not $t1.TimedOut) -and ($t1.ExitCode -eq 0)) -Details $t1.Error
Test-Result -Name 'Non-existent PID prints Matching handles: 0' -Condition ($t1.Output -match 'Matching handles:\s*0')

$t2 = Invoke-HandleEnum -ExePath $exe -Args @('--type', 'FakeType')
Test-Result -Name 'Non-existent type does not crash' -Condition ((-not $t2.TimedOut) -and ($t2.ExitCode -eq 0)) -Details $t2.Error
Test-Result -Name 'Non-existent type yields zero results' -Condition ($t2.Output -match 'Matching handles:\s*0')

Write-Host "`n[2] Access Denied & Privileges"
$t3 = Invoke-HandleEnum -ExePath $exe -Args @('--pid', "$PID", '--verbose', '--count') -TimeoutSeconds 60
Test-Result -Name 'Verbose run does not hang' -Condition ((-not $t3.TimedOut) -and ($t3.ExitCode -eq 0))

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltinRole]::Administrator)
$warnedPrivilege = $t3.Error -match 'failed to enable SeDebugPrivilege'
$privilegeBehaviorOk = $warnedPrivilege -or $isAdmin
Test-Result -Name 'SeDebugPrivilege failure is handled/logged (or running elevated)' -Condition $privilegeBehaviorOk -Details 'Expected warning when not elevated.'

$pidWithNA = Find-PidWithNAName -ExePath $exe
if ($null -eq $pidWithNA) {
    Test-Skip -Name 'Inaccessible handles represented as N/A' -Reason 'No sampled PID produced N/A in this environment.'
} else {
    $t3na = Invoke-HandleEnum -ExePath $exe -Args @('--pid', "$pidWithNA") -TimeoutSeconds 60
    $hasNA = ($t3na.ExitCode -eq 0) -and ($t3na.Output -match '\bN/A\b')
    Test-Result -Name 'Inaccessible handles represented as N/A' -Condition $hasNA
}

Write-Host "`n[3] Filter Logic Integration"
$validPid = $PID
$eventSubstring = Find-EventNameSubstringForPid -ExePath $exe -TargetPid $validPid
if ($null -eq $eventSubstring) {
    Test-Skip -Name 'Triple filter logical AND' -Reason 'No Event handle with queryable name found in current PowerShell process.'
} else {
    $t4 = Invoke-HandleEnum -ExePath $exe -Args @('--pid', "$validPid", '--type', 'Event', '--object', $eventSubstring)
    $rows = Get-DataRows -Output $t4.Output
    $allMatch = $true
    foreach ($row in $rows) {
        if (($row.Pid -ne $validPid) -or
            ($row.Type -ine 'Event') -or
            ($row.Name -inotmatch [regex]::Escape($eventSubstring))) {
            $allMatch = $false
            break
        }
    }

    Test-Result -Name 'Triple filter command exits successfully' -Condition ((-not $t4.TimedOut) -and ($t4.ExitCode -eq 0))
    Test-Result -Name 'Triple filter applies logical AND across pid/type/object' -Condition $allMatch
}

$t5a = Invoke-HandleEnum -ExePath $exe -Args @('--pid', "$validPid", '--type', 'File', '--count')
$t5b = Invoke-HandleEnum -ExePath $exe -Args @('--pid', "$validPid", '--type', 'file', '--count')
$countA = Get-MatchingCount -Output $t5a.Output
$countB = Get-MatchingCount -Output $t5b.Output
$delta = if (($null -ne $countA) -and ($null -ne $countB)) { [Math]::Abs($countA - $countB) } else { 99999 }
Test-Result -Name 'Type filter is case-insensitive (File vs file)' -Condition (($t5a.ExitCode -eq 0) -and ($t5b.ExitCode -eq 0) -and ($null -ne $countA) -and ($null -ne $countB) -and ($delta -le 3)) -Details "CountA=$countA, CountB=$countB, Delta=$delta"

# TEST 5: All filters combined (uses current process PID to avoid PID 4 system-handle hangs)
$t5combined = Invoke-HandleEnum -ExePath $exe -Args @('--pid', "$PID", '--type', 'Event', '--object', 'NonExistent', '--count') -TimeoutSeconds 60
$combinedCount = Get-MatchingCount -Output $t5combined.Output
Test-Result -Name 'All filters combined does not hang' -Condition ((-not $t5combined.TimedOut) -and ($t5combined.ExitCode -eq 0))
Test-Result -Name 'All filters combined returns zero matches for NonExistent object' -Condition (($null -ne $combinedCount) -and ($combinedCount -eq 0))

Write-Host "`n[4] UI & Formatting"
$t6 = Invoke-HandleEnum -ExePath $exe -Args @('--pid', '999999', '--count')
$hasHeader = $t6.Output -match '^\s*PID\s+Process\s+Handle\s+Type\s+Name\s*$'
$hasRows = $t6.Output -match '^\s*\d+\s+.+\s+0x[0-9A-F]+\s+'
$hasSummary = ($t6.Output -match 'Retrieved\s+\d+\s+system handles\.') -and ($t6.Output -match 'Matching handles:\s*0')
Test-Result -Name 'Count-only mode prints summary only (no table header/rows)' -Condition ((-not $hasHeader) -and (-not $hasRows) -and $hasSummary)

$t7 = Invoke-HandleEnum -ExePath $exe -Args @('--pid', "$PID", '--verbose', '--count')
$idxVerbose = $t7.Output.IndexOf('Verbose mode is ON')
$idxRetrieved = $t7.Output.IndexOf('Retrieved ')
Test-Result -Name 'Verbose marker appears before data' -Condition (($idxVerbose -ge 0) -and ($idxRetrieved -gt $idxVerbose))

Write-Host "`n[5] Sorting Edge Cases"
$pidOnlyPlaceholder = Find-PidWithOnlyPlaceholderNames -ExePath $exe
if ($null -eq $pidOnlyPlaceholder) {
    Test-Skip -Name 'Sort by name on only N/A/Locked set' -Reason 'No PID with only placeholder names found among sampled processes.'
} else {
    $t8 = Invoke-HandleEnum -ExePath $exe -Args @('--pid', "$pidOnlyPlaceholder", '--sort', 'name')
    Test-Result -Name 'Sort by name does not crash on identical/placeholder names' -Condition ((-not $t8.TimedOut) -and ($t8.ExitCode -eq 0))
}

Write-Host "`n[6] Resource Cleanup"

# 6a. No orphaned HandleEnum process after repeated runs
for ($i = 0; $i -lt 20; $i++) {
    [void](Invoke-HandleEnum -ExePath $exe -Args @('--count'))
}
Start-Sleep -Milliseconds 300
$leftover = Get-Process -Name 'HandleEnum' -ErrorAction SilentlyContinue
Test-Result -Name 'No orphaned HandleEnum processes after repeated runs' -Condition ($null -eq $leftover)

# 6b. Handle-count growth heuristic in parent shell (not a definitive leak detector)
$beforeHandles = (Get-Process -Id $PID).HandleCount
for ($i = 0; $i -lt 30; $i++) {
    [void](Invoke-HandleEnum -ExePath $exe -Args @('--pid', "$PID", '--type', 'File', '--count'))
}
$afterHandles = (Get-Process -Id $PID).HandleCount
$handleDelta = $afterHandles - $beforeHandles
Test-Result -Name 'No suspicious parent handle growth after repeated invocations (heuristic)' -Condition ($handleDelta -lt 25) -Details "Handle delta: $handleDelta"

Write-Host "`n== Summary =="
Write-Host "Passed:  $script:Passed"
Write-Host "Failed:  $script:Failed"
Write-Host "Skipped: $script:Skipped"

if ($script:Failed -gt 0) {
    exit 1
}

exit 0
