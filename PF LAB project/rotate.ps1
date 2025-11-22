param(
    [string]$in = "$PSScriptRoot\assets\car.png",
    [string]$out = "$PSScriptRoot\assets\car_rotated.png"
)

Add-Type -AssemblyName System.Drawing

if (-not (Test-Path $in)) {
    Write-Error "Input file not found: $in"
    exit 2
}

try {
    $img = [System.Drawing.Image]::FromFile($in)
    $img.RotateFlip([System.Drawing.RotateFlipType]::Rotate90FlipNone)
    $img.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
    $img.Dispose()
    Write-Host "Saved rotated image to: $out"
    exit 0
} catch {
    Write-Error "Failed to rotate image: $_"
    exit 1
}
