$thumb = "4A9A5F218B2B4D90A78274B9C261FED3A9DC78E9"

$locations = @(
    "Cert:\CurrentUser\My",
    "Cert:\CurrentUser\Root",
    "Cert:\CurrentUser\TrustedPublisher",
    "Cert:\LocalMachine\Root",
    "Cert:\LocalMachine\TrustedPublisher"
)

foreach ($loc in $locations) {
    $found = Get-ChildItem $loc -ErrorAction SilentlyContinue | Where-Object { $_.Thumbprint -eq $thumb }
    if ($found) {
        Write-Host "[O] $loc"
    } else {
        Write-Host "[ ] $loc"
    }
}
