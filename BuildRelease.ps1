﻿
## download the last release locally so we can build a delta against it
## Deltas are only ever built against the last previous release, never against more
$squirrel = Get-ChildItem .\Squirrel\squirrel.windows.*\tools\Squirrel.com
$squirrelDll = Get-ChildItem .\Squirrel\squirrel.windows.*\lib\net45\Squirrel.dll

if (-Not(Test-Path -PathType Leaf $squirrel)) {
    Write-Error "Squirrel executable is not present"
    Exit 1
}

$releasesDir = "releases-packages"

if (Test-Path $releasesDir) {
    rm -Force -Recurse $releasesDir
}
mkdir -Force $releasesDir

# Get the latest nuget package
$releasePackage = Join-Path (pwd) "TemplePlus.$($env:TEMPLEPLUS_VERSION).nupkg"

if (!$releasePackage -Or -Not(Test-Path $releasePackage)) {
    Write-Error "NuGet package for release doesnt seem to be built. Make sure to run PackRelease.ps1 first"
    Exit 1
}

# Download last release to make delta packages!
Invoke-WebRequest https://templeplus.org/update-feeds/stable/RELEASES -OutFile $releasesDir\RELEASES

Add-Type -LiteralPath $squirrelDll

# Using squirrel code here to parse the RELEASES file and get the previous release
$releasesContent = Get-Content $releasesDir\RELEASES -Encoding UTF8 | Out-String
$releases = [Squirrel.ReleaseEntry]::ParseReleaseFile($releasesContent)

$rp = New-Object "Squirrel.ReleasePackage" $releasePackage
$prevRelease = [Squirrel.ReleaseEntry]::GetPreviousRelease($releases, $rp, $releasesDir)

if (!$prevRelease) {
    "No previous release found. Picking last full release."
    $prevRelease = $releases | Sort-Object Version -Descending | Where-Object { -not $_.IsDelta } | Select-Object -First 1
    $targetPath = Join-Path $releasesDir $prevRelease.Filename
    $prevRelease = [Squirrel.ReleasePackage]::new($targetPath, $true)
}

"Last Full Release: $($prevRelease.Version.ToString())"

$tagName = "v$($prevRelease.Version.ToString())"
$filenameOnly = [IO.Path]::GetFileName($prevRelease.InputPackageFile)
$downloadUrl = "https://github.com/GrognardsFromHell/TemplePlus/releases/download/$tagName/$filenameOnly"
Invoke-WebRequest $downloadUrl -OutFile $prevRelease.InputPackageFile

# The tag name is actually part of the download URL on github
$baseUrl = "--baseUrl=https://github.com/GrognardsFromHell/TemplePlus/releases/download/v$($env:TEMPLEPLUS_VERSION)/"
"Using BaseURL: $baseUrl"

&$squirrel --releasify=$releasePackage --loadingGif=Configurator\Installing.gif --icon=TemplePlus\toee_gog_icon.ico --setupIcon=TemplePlus\toee_gog_icon.ico --releaseDir=$releasesDir $baseUrl --no-msi

ren "$releasesDir\Setup.exe" "TemplePlusSetup.exe"

# Remove the previous release so it isn't accidentally uploaded as well
if ($prevRelease) {
    Remove-Item $prevRelease.InputPackageFile
}
