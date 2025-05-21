function Get-WordList {
    $url = "https://raw.githubusercontent.com/davidak/wortliste/master/wortliste.txt"
    $words = (Invoke-WebRequest -Uri $url -UseBasicParsing).Content -split "`n"
    return $words | Where-Object { $_.Length -eq 5 } | ForEach-Object { $_.ToLower() }
}

function Show-ColoredText($text, $color) {
    Write-Host $text -ForegroundColor $color -NoNewline
}

# Wörterliste herunterladen
$wordList = Get-WordList

# Zufälliges Wort auswählen
$word = $wordList | Get-Random

# Spiellogik
$attempts = 6
$guessed = $false
$guessHistory = @()

Clear-Host
Write-Host "Willkommen bei Wordle auf Deutsch!" -ForegroundColor Cyan
Write-Host "Errate das 5-buchstabige Wort. Du hast 6 Versuche." -ForegroundColor Cyan
Write-Host ""

while ($attempts -gt 0 -and -not $guessed) {
    $guess = Read-Host "Gib deinen Versuch ein"
    
    if ($guess.Length -ne 5) {
        Write-Host "Bitte gib ein 5-buchstabiges Wort ein." -ForegroundColor Red
        continue
    }

    $result = ""
    for ($i = 0; $i -lt 5; $i++) {
        if ($guess[$i] -eq $word[$i]) {
            $result += "G"
        }
        elseif ($word.Contains($guess[$i])) {
            $result += "Y"
        }
        else {
            $result += "-"
        }
    }

    $guessHistory += ,@($guess, $result)

    Clear-Host
    Write-Host "Wordle auf Deutsch" -ForegroundColor Cyan
    Write-Host ""

    foreach ($pastGuess in $guessHistory) {
        for ($i = 0; $i -lt 5; $i++) {
            $char = $pastGuess[0][$i]
            switch ($pastGuess[1][$i]) {
                "G" { Show-ColoredText " $char " Green }
                "Y" { Show-ColoredText " $char " Yellow }
                "-" { Show-ColoredText " $char " DarkGray }
            }
        }
        Write-Host ""
    }

    Write-Host ""
    if ($guess -eq $word) {
        $guessed = $true
        Write-Host "Gratulation! Du hast das Wort erraten: $word" -ForegroundColor Green
    }
    else {
        $attempts--
        Write-Host "Noch $attempts Versuche übrig." -ForegroundColor Magenta
    }
}

if (-not $guessed) {
    Write-Host "Schade! Das gesuchte Wort war: $word" -ForegroundColor Red
}