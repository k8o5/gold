# Wizard-Spiel gegen KI (Vereinfacht)

# --- Konfiguration ---
$spielername = Read-Host "Bitte gib deinen Namen ein"
$anzahlRunden = 3 # Anzahl der zu spielenden Runden
$handKarten = @()
$kiHandKarten = @()
$kiName = "KI" # Name für den KI-Gegner

# --- Funktionen ---

function GeneriereDeck {
    $farben = @("Rot", "Blau", "Grün", "Gelb")
    $werte = 1..13
    $deck = @()

    foreach ($farbe in $farben) {
        foreach ($wert in $werte) {
            $deck += "$farbe $wert"
        }
    }
    $deck += "Wizard"
    $deck += "Wizard"
    $deck += "Narr"
    $deck += "Narr"
    $deck += "Narr"
    $deck += "Narr"

    return $deck | Get-Random -Count $deck.Count #Mischen
}

function Austeilen {
  param(
    [int]$anzahlKarten,
    [ref]$deck
  )

  $hand = @()
  for ($i = 0; $i -lt $anzahlKarten; $i++) {
    $hand += $deck.Value[0]
    $deck.Value = $deck.Value[1..($deck.Value.Length - 1)]
  }
  return $hand
}

function KI_Vorhersage {
    param(
        [int]$verbleibendeKarten
    )

    #Einfache KI: Rät zufällig zwischen 0 und der Anzahl der verbleibenden Karten.
    return (Get-Random -Minimum 0 -Maximum $verbleibendeKarten)
}

function Spieler_Vorhersage {
    param(
        [int]$verbleibendeKarten
    )

    while ($true) {
        $eingabe = Read-Host "Wie viele Stiche wirst du machen? (0-$verbleibendeKarten)"
        if ($eingabe -match "^\d+$") { #Überprüfen, ob die Eingabe eine Zahl ist
            $vorhersage = [int]$eingabe
            if ($vorhersage -ge 0 -and $vorhersage -le $verbleibendeKarten) {
                return $vorhersage
            } else {
                Write-Host "Ungültige Eingabe. Bitte gib eine Zahl zwischen 0 und $verbleibendeKarten ein."
            }
        } else {
            Write-Host "Ungültige Eingabe. Bitte gib eine Zahl ein."
        }
    }
}

function SpieleRunde {
    param(
        [int]$runde
    )

    Write-Host "--- Runde $runde ---"
    $anzahlKarten = $runde #Anzahl der Karten in dieser Runde

    # 1. Karten austeilen
    $deck = [ref](GeneriereDeck)
    $handKarten = Austeilen -anzahlKarten $anzahlKarten -deck $deck
    $kiHandKarten = Austeilen -anzahlKarten $anzahlKarten -deck $deck


    # 2. Vorhersagen abgeben
    Write-Host "$spielername, deine Karten: $($handKarten -join ', ')"
    $spielerVorhersage = Spieler_Vorhersage -verbleibendeKarten $anzahlKarten
    $kiVorhersage = KI_Vorhersage -verbleibendeKarten $anzahlKarten
    Write-Host "KI sagt $kiVorhersage Stiche voraus."

    # 3. Stiche spielen (stark vereinfacht)
    $spielerStiche = 0
    $kiStiche = 0

    for ($stich = 1; $stich -le $anzahlKarten; $stich++) {
       Write-Host "-- Stich $stich --"

       #Spieler spielt eine Karte (zufällig gewählt, kann verbessert werden)
       $spielerKarteIndex = (Get-Random -Maximum $handKarten.Count)
       $spielerKarte = $handKarten[$spielerKarteIndex]
       $handKarten = $handKarten | Where-Object {$_ -ne $spielerKarte} #Entferne die Karte aus der Hand
       Write-Host "$spielername spielt: $spielerKarte"

       #KI spielt eine Karte (zufällig gewählt, kann verbessert werden)
       $kiKarteIndex = (Get-Random -Maximum $kiHandKarten.Count)
       $kiKarte = $kiHandKarten[$kiKarteIndex]
       $kiHandKarten = $kiHandKarten | Where-Object {$_ -ne $kiKarte} #Entferne die Karte aus der Hand
       Write-Host "KI spielt: $kiKarte"

       # Stich auswerten (sehr einfach)
       if ((Get-Random -Maximum 2) -eq 0) { #Zufällige Stichvergabe.  Kann verbessert werden.
           Write-Host "$spielername gewinnt den Stich!"
           $spielerStiche++
       } else {
           Write-Host "KI gewinnt den Stich!"
           $kiStiche++
       }
    }

    # 4. Punkte vergeben
    $spielerPunkte = BerechnePunkte -vorhersage $spielerVorhersage -stiche $spielerStiche
    $kiPunkte = BerechnePunkte -vorhersage $kiVorhersage -stiche $kiStiche

    Write-Host "$spielername hat $spielerStiche Stiche gemacht (Vorhersage: $spielerVorhersage). Punkte: $spielerPunkte"
    Write-Host "KI hat $kiStiche Stiche gemacht (Vorhersage: $kiVorhersage). Punkte: $kiPunkte"

    return @{ SpielerPunkte = $spielerPunkte; KIPunkte = $kiPunkte }
}

function BerechnePunkte {
    param (
        [int]$vorhersage,
        [int]$stiche
    )

    if ($vorhersage -eq $stiche) {
        return 20 + ($stiche * 10)
    } else {
        return - ([Math]::Abs($vorhersage - $stiche) * 10)
    }
}

# --- Hauptteil des Skripts ---

Write-Host "Willkommen zum Wizard-Spiel gegen die KI!"

$gesamtSpielerPunkte = 0
$gesamtKIPunkte = 0

for ($runde = 1; $runde -le $anzahlRunden; $runde++) {
    $rundenErgebnisse = SpieleRunde -runde $runde
    $gesamtSpielerPunkte += $rundenErgebnisse.SpielerPunkte
    $gesamtKIPunkte += $rundenErgebnisse.KIPunkte
}

Write-Host "--- Endergebnis ---"
Write-Host "${spielername}: $gesamtSpielerPunkte Punkte"
Write-Host "${kiName}: $gesamtKIPunkte Punkte"

if ($gesamtSpielerPunkte -gt $gesamtKIPunkte) {
    Write-Host "${spielername} gewinnt!"
} elseif ($gesamtKIPunkte -gt $gesamtSpielerPunkte) {
    Write-Host "Die KI gewinnt!"
} else {
    Write-Host "Unentschieden!"
}