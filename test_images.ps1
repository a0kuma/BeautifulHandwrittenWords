# Test script to verify image formats and properties
Write-Host "=== Image Format Analysis ===" -ForegroundColor Green

$imageFiles = Get-ChildItem "impool\*.jpg" | Select-Object -First 5

foreach ($file in $imageFiles) {
    Write-Host "`nAnalyzing: $($file.Name)" -ForegroundColor Yellow
    
    # Use ExifTool to get real format
    $exifOutput = & exiftool $file.FullName 2>$null
    
    $fileType = ($exifOutput | Where-Object { $_ -match "^File Type\s*:\s*(.+)" }) -replace "^File Type\s*:\s*", ""
    $imageWidth = ($exifOutput | Where-Object { $_ -match "^Image Width\s*:\s*(.+)" }) -replace "^Image Width\s*:\s*", ""
    $imageHeight = ($exifOutput | Where-Object { $_ -match "^Image Height\s*:\s*(.+)" }) -replace "^Image Height\s*:\s*", ""
    $fileSize = ($exifOutput | Where-Object { $_ -match "^File Size\s*:\s*(.+)" }) -replace "^File Size\s*:\s*", ""
    
    Write-Host "  File Type: $fileType" -ForegroundColor Cyan
    Write-Host "  Dimensions: ${imageWidth}x${imageHeight}" -ForegroundColor Cyan
    Write-Host "  File Size: $fileSize" -ForegroundColor Cyan
    
    # Check for potential issues
    if ($fileType -eq "WEBP" -and $file.Extension -eq ".jpg") {
        Write-Host "  ⚠️  WARNING: WebP file with .jpg extension!" -ForegroundColor Red
    }
}

Write-Host "`n=== Summary ===" -ForegroundColor Green
Write-Host "Check the ImageViewer console output for detailed loading information."
