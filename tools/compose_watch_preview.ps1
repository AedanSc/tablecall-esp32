Add-Type -AssemblyName System.Drawing
$projectPath = Split-Path $PSScriptRoot -Parent
$sheet = New-Object System.Drawing.Bitmap 1400,820
$graphics = [System.Drawing.Graphics]::FromImage($sheet)
$graphics.Clear([System.Drawing.Color]::FromArgb(242,245,249))
$graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
$graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
$font = New-Object System.Drawing.Font 'Segoe UI',22,([System.Drawing.FontStyle]::Bold),([System.Drawing.GraphicsUnit]::Pixel)
$tiles = @(@('idle','ALL CAUGHT UP',40,60),@('new','NEW REQUESTS',720,60),@('coming','WAITER COMING',40,450),@('sync','COMPLETION CONFIRMATION',720,450))
foreach ($tile in $tiles) {
  $imagePath = Join-Path $projectPath ('.pio/watch-' + $tile[0] + '.bmp')
  $screen = [System.Drawing.Image]::FromFile($imagePath)
  $xx = [int]$tile[2]; $yy = [int]$tile[3]
  $graphics.DrawString($tile[1],$font,[System.Drawing.Brushes]::Black,$xx,($yy-36))
  $graphics.DrawImage($screen,(New-Object System.Drawing.Rectangle $xx,$yy,640,320),0,0,128,64,([System.Drawing.GraphicsUnit]::Pixel))
  $screen.Dispose()
}
$sheet.Save((Join-Path $projectPath 'docs/watch-display-preview.png'),[System.Drawing.Imaging.ImageFormat]::Png)
$font.Dispose(); $graphics.Dispose(); $sheet.Dispose()
Write-Output 'Created docs/watch-display-preview.png (5x actual pixels).'
