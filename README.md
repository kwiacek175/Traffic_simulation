# Traffic Simulation System

## Opis

Projekt "Traffic Simulation System" jest symulatorem ruchu drogowego z uwzględnieniem toru okrężnego oraz toru kolizyjnego. Symulacja obejmuje pojazdy poruszające się po torze, interakcje z skrzyżowaniami oraz obsługę torów kolizyjnych. Projekt wykorzystuje wątki i synchronizację, aby zapewnić realistyczne odwzorowanie ruchu i unikanie kolizji.

## Funkcjonalności

- **Rysowanie toru**: Wizualizacja toru okrężnego oraz toru kolizyjnego na ekranie.
- **Pojazdy**: Pojazdy poruszające się po torze, z możliwością zmiany prędkości i przyspieszenia.
- **Synchronizacja**: Zarządzanie dostępnością skrzyżowań i torów, aby uniknąć kolizji.
- **Wielowątkowość**: Wykorzystanie wątków do zarządzania ruchem pojazdów oraz ich rysowaniem na ekranie.
- **Zarządzanie pojazdami kolizyjnymi**: Dodawanie nowych pojazdów kolizyjnych i ich ruchu wzdłuż toru kolizyjnego.

## Jak uruchomić

1. **Klonowanie repozytorium**

    ```bash
    git clone https://github.com/yourusername/traffic-simulation-system.git
    ```

2. **Przejdź do katalogu projektu**

    ```bash
    cd traffic-simulation-system
    ```

3. **Kompilacja**

    Upewnij się, że masz zainstalowane odpowiednie biblioteki (np. `ncurses`), a następnie skompiluj kod źródłowy:

    ```bash
    g++ -o traffic_simulation main.cpp -lncurses -pthread
    ```

4. **Uruchomienie**

    Po skompilowaniu uruchom program:

    ```bash
    ./traffic_simulation
    ```

5. **Sterowanie**

    Aby zakończyć symulację, naciśnij klawisz `SPACE`.

## Wymagania

- **System operacyjny**: Linux lub inny system obsługujący bibliotekę `ncurses`.
- **Biblioteki**: `ncurses` do wyświetlania na ekranie, `pthread` do obsługi wątków.




