function Show-Title {
    Clear-Host
    Write-Host "`n`n"
    Write-Host "╔══════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║             BLACKJACK                ║" -ForegroundColor Cyan
    Write-Host "╚══════════════════════════════════════╝" -ForegroundColor Cyan
    Write-Host "`n"
}

function Get-Card {
    $suits = "♥", "♦", "♣", "♠"
    $values = "A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"
    
    $card = @{
        Suit = Get-Random -InputObject $suits
        Value = Get-Random -InputObject $values
    }
    return $card
}

function Get-CardValue {
    param ($card)
    
    switch ($card.Value) {
        "A" { return 11 }
        { $_ -in "K","Q","J" } { return 10 }
        default { return [int]$card.Value }
    }
}

function Show-Hand {
    param (
        $hand,
        $player,
        $hideSecond = $false
    )
    
    Write-Host "$player's hand: " -NoNewline
    
    for ($i = 0; $i -lt $hand.Count; $i++) {
        if ($hideSecond -and $i -eq 1) {
            Write-Host "🂠 " -NoNewline -ForegroundColor Yellow
        } else {
            $color = if ($hand[$i].Suit -in "♥","♦") { "Red" } else { "White" }
            Write-Host "$($hand[$i].Suit)$($hand[$i].Value) " -NoNewline -ForegroundColor $color
        }
    }
    Write-Host "`n"
}

function Get-HandValue {
    param ($hand)
    
    $value = 0
    $aces = 0
    
    foreach ($card in $hand) {
        if ($card.Value -eq "A") {
            $aces++
        }
        $value += Get-CardValue $card
    }
    
    while ($value -gt 21 -and $aces -gt 0) {
        $value -= 10
        $aces--
    }
    
    return $value
}

function Play-Hand {
    param (
        $hand,
        $bet,
        [ref]$money
    )

    $playerBust = $false
    $playerValue = 0
    
    while ($true) {
        Show-Title
        Show-Hand $dealerHand "Dealer" -hideSecond $true
        Show-Hand $hand "Player"
        
        $playerValue = Get-HandValue $hand
        Write-Host "Your hand value: $playerValue`n"
        
        if ($playerValue -gt 21) {
            Write-Host "BUST! You lose!" -ForegroundColor Red
            $playerBust = $true
            $money.Value -= $bet
            break
        }
        
        $action = Read-Host "Do you want to (H)it or (S)tand?"
        if ($action.ToUpper() -eq "S") { break }
        if ($action.ToUpper() -eq "H") {
            $hand += Get-Card
        }
    }
    
    return @{
        Bust = $playerBust
        Value = $playerValue
        Hand = $hand
    }
}

$money = 1000
while ($money -gt 0) {
    Show-Title
    Write-Host "Your current balance: `$$money`n" -ForegroundColor Green
    
    $bet = Read-Host "Enter your bet (0 to quit)"
    if ($bet -eq "0") { break }
    if ([int]$bet -gt $money) {
        Write-Host "You don't have enough money!" -ForegroundColor Red
        Start-Sleep -Seconds 2
        continue
    }
    
    $playerHand = @()
    $dealerHand = @()
    
    # Initial deal
    $playerHand += Get-Card
    $dealerHand += Get-Card
    $playerHand += Get-Card
    $dealerHand += Get-Card
    
    # Check for split possibility
    $canSplit = ($playerHand[0].Value -eq $playerHand[1].Value) -and ($money -ge $bet * 2)
    
    # Check for player blackjack
    $playerBlackjack = (Get-HandValue $playerHand) -eq 21
    $dealerBlackjack = (Get-HandValue $dealerHand) -eq 21

    Show-Title
    Show-Hand $dealerHand "Dealer" -hideSecond $true
    Show-Hand $playerHand "Player"

    if ($playerBlackjack) {
        if ($dealerBlackjack) {
            Write-Host "Both have Blackjack! Push!" -ForegroundColor Yellow
            Start-Sleep -Seconds 2
            continue
        }
        Write-Host "BLACKJACK! You win!" -ForegroundColor Green
        $money += [math]::Floor($bet * 1.5) # 3:2 payout for blackjack
        Start-Sleep -Seconds 2
        continue
    }

    if ($canSplit) {
        $splitDecision = Read-Host "Do you want to split your hand? (Y/N)"
        if ($splitDecision.ToUpper() -eq "Y") {
            $hand1 = @($playerHand[0], (Get-Card))
            $hand2 = @($playerHand[1], (Get-Card))
            
            Write-Host "`nPlaying first hand:" -ForegroundColor Yellow
            $result1 = Play-Hand $hand1 $bet ([ref]$money)
            
            Write-Host "`nPlaying second hand:" -ForegroundColor Yellow
            $result2 = Play-Hand $hand2 $bet ([ref]$money)
            
            if (-not ($result1.Bust -and $result2.Bust)) {
                $dealerValue = Get-HandValue $dealerHand
                while ($dealerValue -lt 17) {
                    $dealerHand += Get-Card
                    $dealerValue = Get-HandValue $dealerHand
                }
                
                Show-Title
                Show-Hand $dealerHand "Dealer"
                Write-Host "Dealer's hand value: $dealerValue`n"
                
                # Process first hand
                if (-not $result1.Bust) {
                    Write-Host "First hand ($($result1.Value)):" -NoNewline
                    if ($dealerValue -gt 21 -or $result1.Value -gt $dealerValue) {
                        Write-Host " Win!" -ForegroundColor Green
                        $money += $bet
                    }
                    elseif ($result1.Value -lt $dealerValue) {
                        Write-Host " Lose!" -ForegroundColor Red
                    }
                    else {
                        Write-Host " Push!" -ForegroundColor Yellow
                        $money += $bet
                    }
                }
                
                # Process second hand
                if (-not $result2.Bust) {
                    Write-Host "Second hand ($($result2.Value)):" -NoNewline
                    if ($dealerValue -gt 21 -or $result2.Value -gt $dealerValue) {
                        Write-Host " Win!" -ForegroundColor Green
                        $money += $bet
                    }
                    elseif ($result2.Value -lt $dealerValue) {
                        Write-Host " Lose!" -ForegroundColor Red
                    }
                    else {
                        Write-Host " Push!" -ForegroundColor Yellow
                        $money += $bet
                    }
                }
            }
        }
        else {
            # Normal play if split is declined
            $result = Play-Hand $playerHand $bet ([ref]$money)
            if (-not $result.Bust) {
                $dealerValue = Get-HandValue $dealerHand
                while ($dealerValue -lt 17) {
                    $dealerHand += Get-Card
                    $dealerValue = Get-HandValue $dealerHand
                }
                
                Show-Title
                Show-Hand $dealerHand "Dealer"
                Show-Hand $playerHand "Player"
                
                Write-Host "Dealer's hand value: $dealerValue"
                Write-Host "Your hand value: $($result.Value)`n"
                
                if ($dealerValue -gt 21) {
                    Write-Host "Dealer BUST! You win!" -ForegroundColor Green
                    $money += $bet
                }
                elseif ($result.Value -gt $dealerValue) {
                    Write-Host "You win!" -ForegroundColor Green
                    $money += $bet
                }
                elseif ($result.Value -lt $dealerValue) {
                    Write-Host "Dealer wins!" -ForegroundColor Red
                    $money -= $bet
                }
                else {
                    Write-Host "Push! It's a tie!" -ForegroundColor Yellow
                }
            }
        }
    }
    else {
        # Normal play without split option
        $result = Play-Hand $playerHand $bet ([ref]$money)
        if (-not $result.Bust) {
            $dealerValue = Get-HandValue $dealerHand
            while ($dealerValue -lt 17) {
                $dealerHand += Get-Card
                $dealerValue = Get-HandValue $dealerHand
            }
            
            Show-Title
            Show-Hand $dealerHand "Dealer"
            Show-Hand $playerHand "Player"
            
            Write-Host "Dealer's hand value: $dealerValue"
            Write-Host "Your hand value: $($result.Value)`n"
            
            if ($dealerValue -gt 21) {
                Write-Host "Dealer BUST! You win!" -ForegroundColor Green
                $money += $bet
            }
            elseif ($result.Value -gt $dealerValue) {
                Write-Host "You win!" -ForegroundColor Green
                $money += $bet
            }
            elseif ($result.Value -lt $dealerValue) {
                Write-Host "Dealer wins!" -ForegroundColor Red
                $money -= $bet
            }
            else {
                Write-Host "Push! It's a tie!" -ForegroundColor Yellow
            }
        }
    }
    
    Start-Sleep -Seconds 2
}

Write-Host "`nGame Over! Final balance: `$$money" -ForegroundColor Cyan
Start-Sleep -Seconds 3