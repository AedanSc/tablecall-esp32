Add-Type -AssemblyName System.Drawing
$projectPath = Split-Path $PSScriptRoot -Parent
$outputPath = Join-Path $projectPath 'docs'
New-Item -ItemType Directory -Path $outputPath -Force | Out-Null
$bitmap = New-Object System.Drawing.Bitmap 1600,1900
$canvas = [System.Drawing.Graphics]::FromImage($bitmap)
$canvas.Clear([System.Drawing.Color]::White)
$canvas.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$canvas.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
$svg = [System.Collections.Generic.List[string]]::new()
$svg.Add('<svg xmlns="http://www.w3.org/2000/svg" width="1600" height="1900" viewBox="0 0 1600 1900"><rect width="1600" height="1900" fill="white"/>')
function Color($value) { [System.Drawing.ColorTranslator]::FromHtml($value) }
function Line($x1,$y1,$x2,$y2,$color='#334155',$width=3) {
  $pen = New-Object System.Drawing.Pen (Color $color),$width
  $canvas.DrawLine($pen,[single]$x1,[single]$y1,[single]$x2,[single]$y2); $pen.Dispose()
  $svg.Add("<line x1='$x1' y1='$y1' x2='$x2' y2='$y2' stroke='$color' stroke-width='$width'/>")
}
function Box($x,$y,$w,$h,$fill='#ffffff',$stroke='#cbd5e1') {
  $brush = New-Object System.Drawing.SolidBrush (Color $fill)
  $pen = New-Object System.Drawing.Pen (Color $stroke),2
  $canvas.FillRectangle($brush,$x,$y,$w,$h); $canvas.DrawRectangle($pen,$x,$y,$w,$h)
  $brush.Dispose(); $pen.Dispose()
  $svg.Add("<rect x='$x' y='$y' width='$w' height='$h' fill='$fill' stroke='$stroke' stroke-width='2'/>")
}
function Text($x,$y,$value,$size=21,$color='#0f172a',$bold=$false) {
  $style = [System.Drawing.FontStyle]::Regular
  if ($bold) { $style = [System.Drawing.FontStyle]::Bold }
  $font = New-Object System.Drawing.Font 'Segoe UI',$size,$style,([System.Drawing.GraphicsUnit]::Pixel)
  $brush = New-Object System.Drawing.SolidBrush (Color $color)
  $canvas.DrawString($value,$font,$brush,[single]$x,[single]$y)
  $escaped = [System.Security.SecurityElement]::Escape($value)
  $weight = if ($bold) { '700' } else { '400' }
  $baseline = $y + $size
  $svg.Add("<text x='$x' y='$baseline' font-family='Segoe UI,Arial,sans-serif' font-size='$size' font-weight='$weight' fill='$color'>$escaped</text>")
  $brush.Dispose(); $font.Dispose()
}
function Dot($x,$y) {
  $canvas.FillEllipse([System.Drawing.Brushes]::SlateGray,($x-4),($y-4),8,8)
  $svg.Add("<circle cx='$x' cy='$y' r='4' fill='#64748b'/>")
}
function Ground($x,$y) {
  Line $x $y $x ($y+12) '#475569'
  Line ($x-18) ($y+12) ($x+18) ($y+12) '#475569'
  Line ($x-12) ($y+20) ($x+12) ($y+20) '#475569'
  Line ($x-5) ($y+28) ($x+5) ($y+28) '#475569'
}
function Resistor($x,$y,$label) {
  Line ($x-20) $y $x $y
  Box $x ($y-10) 70 20 '#ffffff' '#334155'
  Line ($x+70) $y ($x+90) $y
  Text ($x-5) ($y-40) $label 19
}
function DrawSwitch($x,$y) {
  Dot $x $y; Dot ($x+60) $y
  Line $x $y ($x+48) ($y-22)
  Line ($x+25) ($y-37) ($x+25) ($y-23)
}
function Arrow($x,$y,$xx,$yy,$color='#334155') {
  Line $x $y $xx $yy $color 2
  Line $xx $yy ($xx-9) ($yy+2) $color 2
  Line $xx $yy ($xx-2) ($yy+9) $color 2
}
function Led($x,$y) {
  # LED diode: anode at left, cathode bar at right; outward arrows show light.
  Line $x $y ($x+20) $y
  Line ($x+20) ($y-15) ($x+20) ($y+15)
  Line ($x+20) ($y-15) ($x+45) $y
  Line ($x+20) ($y+15) ($x+45) $y
  Line ($x+45) ($y-15) ($x+45) ($y+15)
  Line ($x+45) $y ($x+70) $y
  Arrow ($x+28) ($y-18) ($x+42) ($y-32)
  Arrow ($x+45) ($y-16) ($x+59) ($y-30)
}

Text 45 20 'TABLE REQUEST SYSTEM | No-transistor wiring' 34 '#0f172a' $true
Text 45 65 'Direct buzzer drive is CONDITIONAL: verify the actual buzzer current at 3.3 V before connecting it.' 21 '#9a3412'

Box 40 115 1520 620 '#f8fafc'
Text 65 130 '1  TABLE UNIT - ESP32 sender' 27 '#0369a1' $true
Text 65 170 'Power this board by USB. Buttons use INPUT_PULLUP; a press connects the GPIO to GND.' 20
Box 90 230 270 450 '#e0f2fe' '#0284c7'
Text 113 245 'ESP32 Dev Module' 23 '#075985' $true
Text 113 278 'TABLE / sender' 19 '#075985'
$buttonRows = @(@(330,'GPIO 26','WAITER / service button'),@(405,'GPIO 27','WATER button'),@(480,'GPIO 25','CHECK button'))
foreach ($row in $buttonRows) {
  $y=[int]$row[0]
  Text 250 ($y-26) $row[1] 20
  Line 360 $y 650 $y '#0284c7'
  DrawSwitch 650 $y
  Line 710 $y 900 $y '#0284c7'; Ground 900 $y
  Text 970 ($y-15) $row[2] 23 '#0f172a' $true
}
$ledRows = @(@(555,'GPIO 21','Waiter LED'),@(610,'GPIO 18','Water LED'),@(665,'GPIO 19','Check LED'))
foreach ($row in $ledRows) {
  $y=[int]$row[0]
  Text 250 ($y-25) $row[1] 20
  Line 360 $y 510 $y '#0284c7'; Resistor 530 $y '330 ohm'
  Line 620 $y 720 $y '#0284c7'; Led 720 $y
  Line 790 $y 900 $y '#0284c7'; Ground 900 $y
  Text 970 ($y-12) $row[2] 22 '#0f172a' $true
}
Text 100 690 'GND pins: all ground symbols in this panel connect to the sender GND.' 18 '#475569'

Text 140 749 'TABLE UNIT  <---------- ESP-NOW wireless link ---------->  WAITER UNIT' 22 '#0369a1' $true

Box 40 795 1520 865 '#f8fafc'
Text 65 812 '2  WAITER WATCH - ESP32 receiver' 27 '#0369a1' $true
Text 65 853 'Power this board by USB. OLED: GME12864-11-12-13 V3.22, SSD1306 I2C, 128 x 64. Follow pin labels.' 20
Box 90 915 270 525 '#e0f2fe' '#0284c7'
Text 113 925 'ESP32 Dev Module' 23 '#075985' $true
Box 1050 915 440 305 '#ecfdf5' '#059669'
Text 1080 928 'SSD1306 I2C OLED' 24 '#047857' $true
Text 1080 960 '0.96 inch / 128 x 64' 19 '#047857'
$oledRows = @(@(1000,'3V3','VCC / 3.3 V','#dc2626'),@(1050,'GND','GND','#475569'),@(1100,'GPIO 14','SCL','#0284c7'),@(1150,'GPIO 13','SDA','#0284c7'))
foreach ($row in $oledRows) {
  $y=[int]$row[0]
  Text 250 ($y-23) $row[1] 19
  Line 360 $y 1050 $y $row[3]
  Text 1080 ($y-22) $row[2] 20
}
Text 250 1236 'GPIO 27' 20
Line 360 1260 550 1260 '#0284c7'; DrawSwitch 550 1260
Line 610 1260 685 1260 '#0284c7'; Ground 685 1260
Text 405 1299 'ONE watch button' 22 '#0f172a' $true
Text 405 1328 '1st press: coming | 2nd: fulfilled' 19
Text 250 1375 'GPIO 25' 20
Line 360 1400 900 1400 '#0284c7'
Text 470 1358 '3.3 V GPIO output -> red (+)' 22 '#0369a1' $true
Box 900 1350 300 100 '#fff7ed' '#ea580c'
Text 925 1365 'SFM-27 active buzzer' 23 '#9a3412' $true
Text 925 1403 'Current compatibility UNVERIFIED' 17 '#9a3412'
Line 1200 1400 1360 1400 '#475569'; Ground 1360 1400
Text 1235 1358 'Black (-)' 21
Box 95 1480 1400 135 '#fff7ed' '#ea580c'
Text 115 1495 'BEFORE CONNECTING THE BUZZER' 23 '#9a3412' $true
Text 115 1530 'Confirm steady and startup current at 3.3 V are within the ESP32 GPIO drive specification.' 21
Text 115 1563 'No transistor, 1 kohm resistor, or external buzzer supply in this version. Never apply 5 V or battery voltage to GPIO 25.' 20
Text 100 1620 'All GND symbols in this panel connect to receiver GND. Firmware still switches GPIO 25 HIGH for a short beep.' 19 '#475569'

Text 45 1685 'BUILD NOTES' 24 '#0f172a' $true
Text 45 1722 '1. Each LED has its own 330 ohm resistor. LED anode (+) is on the left; cathode (- / flat side) on the right.' 21
Text 45 1756 '2. Four-leg tactile buttons: use contacts that are open when released and connected when pressed.' 21
Text 45 1790 '3. GND symbols are joined within each unit. No wired connection is required between table and watch.' 21
Text 45 1824 '4. GPIO numbers repeat on the two boards. Follow board labels; this is a logical schematic, not a pin-position map.' 20
Text 45 1858 'Parts: 2 ESP32s, 4 buttons, 3 LEDs, 3 x 330 ohm, SSD1306 OLED, GPIO-compatible active buzzer (verify SFM-27 first).' 20

$svg.Add('</svg>')
[System.IO.File]::WriteAllLines((Join-Path $outputPath 'wiring-schematic.svg'),$svg,[System.Text.UTF8Encoding]::new($false))
$bitmap.Save((Join-Path $outputPath 'wiring-schematic.png'),[System.Drawing.Imaging.ImageFormat]::Png)
$canvas.Dispose(); $bitmap.Dispose()
Write-Output 'Created docs/wiring-schematic.svg and docs/wiring-schematic.png'
