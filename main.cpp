// ETAP II
// Na obu skrzyżowaniach ma się znajdować maksymalnie 1 pojazd.
// Na prawym odcinku toru mają się znajdować maksymalnie 3 pojazdy.

#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <ncurses.h>
#include <chrono>
#include <atomic>
#include <algorithm> 
#include <condition_variable>

const int TRACK_WIDTH = 75;             // Szerokość toru okrężnego 
const int TRACK_HEIGHT = 19;            // Długość toru okrężnego 
const int ROAD_WIDTH = 6;               // Szerokość drogi w torze
const int COLISION_ROAD_HEIGHT = 23;    // Długość toru kolizyjnego 

// Struktura przechowująca informacje o pojeździe
struct Vehicle {
    int x, y;                           // pozycja pojazdu
    char symbol;                        // symbol pojazdu
    int color;                          // kolor pojazdu
    int speed;                          // prędkość pojazdu
    int acceleration;                   // przyspieszenie pojazdu
    std::atomic<bool> active; 

    // Konstruktor
    Vehicle(int px, int py, char sym, int col, int v, int a) : x(px), y(py), symbol(sym), color(col), speed(v), acceleration(a), active(true) {}

    // Konstruktor przenoszący
    Vehicle(Vehicle&& other) noexcept
        : x(other.x), y(other.y), symbol(other.symbol), color(other.color), speed(other.speed), acceleration(other.acceleration), active(other.active.load())  {

         other.active = false;   
    }


    // Operator przypisania przenoszącego
    Vehicle& operator=(Vehicle&& other) noexcept {
        if (this != &other) {
            x = other.x;
            y = other.y;
            symbol = other.symbol;
            color = other.color;
            speed = other.speed;
            acceleration = other.acceleration;
            active.store(other.active.load());

            other.active = false;
        }
        return *this;
    }
    

    // Usunięcie konstruktora kopiującego i operatora przypisania kopiującego
    Vehicle(const Vehicle&) = delete;
    Vehicle& operator=(const Vehicle&) = delete;
};

std::vector<Vehicle> vehicles;                       // Wektor przechowujący pojazdy na torze 
std::vector<Vehicle> collisionVehicles;              // Wektor przechowujący pojazdy kolizyjne
std::vector<std::thread> vehicleThreads;             // Wektor przechowujący wątki pojazdów na torze 
std::vector<std::thread> collisionVehicleThreads;    // Wektor przechowujący wątki pojazdów kolizyjnych 

std::mutex vehiclesMutex;                            // mutex do synchronizacji dostępu do wektora pojazdów
std::mutex collisionVehiclesMutex;                   // mutex do synchronizacji dostępu do wektora pojazdów kolizyjnych
std::mutex crossingMutex1;                           // mutex do synchronizacji pierwszego skrzyżowania
std::mutex crossingMutex2;                           // mutex do synchronizacji drugiego skrzyżowania
std::mutex rightTrackMutex;                          // mutex do synchronizacji dostępu do prawego odcinka toru

std::condition_variable crossing1AvailableCV;        // Warunek dostępności skrzyżowania 1
bool crossing1Available = true;

std::condition_variable crossing2AvailableCV;        // Warunek dostępności skrzyżowania 2
bool crossing2Available = true;

std::condition_variable rightTrackAvailableCV;            // Warunek dostępności obszaru toru
bool rightTrackAvailable = true;                          // Flaga oznaczająca dostępność prawego toru
int rightTrackCount = 0;                                  // Licznik wątków na prawym torze
              
volatile std::atomic<bool> running(true);       // Flaga sygnalizująca czy program powinien nadal działać

// Funkcja rysująca tor okrężny
void drawTrack() {
    // Rysowanie zewnętrznej linii toru na prawej i lewej linii 
    for (int i = 2; i <= TRACK_HEIGHT + 2; ++i) {
        mvprintw(i, 0, "|");
        mvprintw(i, TRACK_WIDTH + 1, "|");
    }
    
    // Rysowanie zewnętrznej linii toru na górnej i dolnej linii 
    for (int i = 1; i < TRACK_WIDTH + 1; ++i) {
        mvprintw(1, i, "_");
        mvprintw(TRACK_HEIGHT + 2, i, "_");
    }
    
    //Rysowanie krawędzi toru 
    mvprintw(4, ROAD_WIDTH, "+");
    mvprintw(4, TRACK_WIDTH - ROAD_WIDTH + 1, "+");
    mvprintw(TRACK_HEIGHT , ROAD_WIDTH, "+");
    mvprintw(TRACK_HEIGHT, TRACK_WIDTH - ROAD_WIDTH + 1, "+");

    // Rysowanie wewnętrznej linii toru na górnej i dolnej linii
    for (int i = 1; i < TRACK_WIDTH - ROAD_WIDTH - 5;  ++i) {
        mvprintw(4, i + ROAD_WIDTH, "-");
        mvprintw(TRACK_HEIGHT, i + ROAD_WIDTH, "-");
    }
    // Rysowanie wewnętrznej linii toru po lewej i prawej stronie 
    for (int i = 5; i <= TRACK_HEIGHT - 1; ++i) {
        mvprintw(i, ROAD_WIDTH, "|");
        mvprintw(i, TRACK_WIDTH - ROAD_WIDTH + 1, "|");
    }
    
    // Rysowanie przejazdu dla toru kolizyjnego 
    mvprintw(1,47,"   ");
    mvprintw(TRACK_HEIGHT, 47, "   ");
    mvprintw(4,47,"   ");
    mvprintw(COLISION_ROAD_HEIGHT - 2, 47,"   ");
}

// Funkcja rysująca dorgę kolizyjną
void drawCollisionRoad(){
    for (int i = 0; i <= COLISION_ROAD_HEIGHT; ++i) {
        mvprintw(i, 45, "|");
        mvprintw(i, 45 + ROAD_WIDTH, "|");
    }
    mvprintw(3, 45, " ");
    mvprintw(3, 51, " ");
    mvprintw(TRACK_HEIGHT + 1, 45, " ");
    mvprintw(TRACK_HEIGHT + 1, 51, " ");
}

// Funkcja rysująca pojazd na ekranie
void drawVehicle(const Vehicle& vehicle) {
    attron(COLOR_PAIR(vehicle.color));                      // Ustawienie koloru pojazdu
    mvaddch(vehicle.y, vehicle.x, vehicle.symbol);
    attroff(COLOR_PAIR(vehicle.color));                     // Wyłączenie koloru
}

// Funkcja wątek - wyświetlanie i odświeżanie ekranu 
void refreshScreen() {
    while (running) {
        clear();
        //erase(); // Wyczyszczenie ekranu
        drawTrack();
        drawCollisionRoad();

        // Rysujemy istniejące pojazdy
        {
            std::lock_guard<std::mutex> lock(vehiclesMutex);
            for (auto& vehicle : vehicles) {
                drawVehicle(vehicle);
            }
        }
        
        // Rysujemy pojazdy kolizyjne
        {
            std::lock_guard<std::mutex> lock(collisionVehiclesMutex);
             for (auto& collisionVehicle : collisionVehicles) {
                 drawVehicle(collisionVehicle);
            }   
        }
        
        refresh(); // Odświeżenie ekranu
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Usypianie na krótki czas
    }
}

// Funkcja wątek - odpowiedzialna za ruch pojazdu
void moveVehicle(Vehicle& vehicle) {

    enum Direction { UP, RIGHT, DOWN, LEFT };
    Direction dir = RIGHT; // Początkowy kierunek ruchu

    while (vehicle.active) {
        // Usypiamy wątek na czas odpowiadający prędkości pojazdu
        std::this_thread::sleep_for(std::chrono::milliseconds(vehicle.speed));

        // Próba przejazdu przez skrzyżowanie 1
        if (vehicle.x == 45 && vehicle.y == 3) {
            std::unique_lock<std::mutex> lock(crossingMutex1);
            crossing1AvailableCV.wait(lock, [&] { return crossing1Available; });
            crossing1Available = false;
        }

        // Jeśli pojazd jest na pozycji x=51,y=3, czeka na wjazd do toru
        if (vehicle.x == 51 && vehicle.y == 3) {
            std::unique_lock<std::mutex> lock(rightTrackMutex);
            rightTrackAvailableCV.wait(lock, []{ return rightTrackAvailable; });
            rightTrackCount++;
            if(rightTrackCount == 3){
                rightTrackAvailable = false;
            }
        }

        // Próba przejazdu przez skrzyżowanie 2
        if (vehicle.x == 51 && vehicle.y == 20) {
          std::unique_lock<std::mutex> lock(crossingMutex2);
          crossing2AvailableCV.wait(lock, [&] { return crossing2Available; });
          crossing2Available = false;
        }
    
        // Sprawdzamy kierunek ruchu pojazdu i aktualizujemy jego pozycję
        switch (dir) {
            case UP:
                if (vehicle.y >= 4) 
                    vehicle.y--;
                else
                    dir = RIGHT; 
                break;
            case RIGHT:
                if (vehicle.x <= TRACK_WIDTH - 3)
                    vehicle.x++;
                else
                    dir = DOWN;
                break;
            case DOWN:
                if (vehicle.y <= TRACK_HEIGHT)
                    vehicle.y++;
                else
                    dir = LEFT;
                break;
            case LEFT:
                if (vehicle.x >= 4)
                    vehicle.x--;
                else
                    dir = UP;
                break;
        }

        // Zwolnienie skrzyżowania 1 po przejeździe
        if (vehicle.x == 51 && vehicle.y == 3) {
            std::unique_lock<std::mutex> lock(crossingMutex1);
            crossing1Available = true;
            crossing1AvailableCV.notify_one();
        }
 
        // Jeśli pojazd jest na pozycji x=52,y=20, opuszcza tor
        if (vehicle.x == 51 && vehicle.y == 20) {
            std::unique_lock<std::mutex> lock(rightTrackMutex);
            rightTrackCount--;
            rightTrackAvailable = true;
            rightTrackAvailableCV.notify_one();
        }

        // Zwolnienie skrzyżowania 2 po przejeździe
        if (vehicle.x == 45 && vehicle.y == 20) {
            std::unique_lock<std::mutex> lock(crossingMutex2);
            crossing2Available = true;
            crossing2AvailableCV.notify_one();
        }
    }
}

// Funkcja wątek - odpowiedzialna za ruch pojazdu kolizyjnego
void moveCollisionVehicle(Vehicle& vehicle) {
    while (vehicle.active) {
        
        // Usypiamy wątek na czas odpowiadający prędkości pojazdu
        std::this_thread::sleep_for(std::chrono::milliseconds(vehicle.speed));

        // Próba przejazdu przez skrzyżowanie 1
        if (vehicle.x == 48 && vehicle.y == 1) {
            std::unique_lock<std::mutex> lock(crossingMutex1);
            crossing1AvailableCV.wait(lock, [&] { return crossing1Available; });
            crossing1Available = false;
        }

        // Próba przejazdu przez skrzyżowanie 2
        if (vehicle.x == 48 && vehicle.y == 19) {
            std::unique_lock<std::mutex> lock(crossingMutex2);
            crossing2AvailableCV.wait(lock, [&] { return crossing2Available; });
            crossing2Available = false;
        }

        // Aktualizujemy pozycję pojazdu (pojazd spada w dół)
        vehicle.y ++; 
        vehicle.speed += vehicle.acceleration;


        // Zwolnienie skrzyżowania 1 po przejeździe
        if (vehicle.x == 48 && vehicle.y == 5) {
            std::unique_lock<std::mutex> lock(crossingMutex1);
            crossing1Available = true;
            crossing1AvailableCV.notify_one();
        }


        // Zwolnienie skrzyżowania 2 po przejeździe
        if (vehicle.x == 48 && vehicle.y == 22) {
            std::unique_lock<std::mutex> lock(crossingMutex2);
            crossing2Available = true;
            crossing2AvailableCV.notify_one();
        }

    
        // Jeśli pojazd przekroczy dolny kraniec toru kolizyjnego, usuwamy go
        if (vehicle.y > COLISION_ROAD_HEIGHT) {
           vehicle.active = false;
           break;
        }

    }
}

// Funkcja wątek - czyszczenie pojazdów kolizyjnych i torowych
void manageVehicles() {
    while (running) { 
        std::this_thread::sleep_for(std::chrono::seconds(1));           
        
        {
            std::lock_guard<std::mutex> lock(collisionVehiclesMutex);
            collisionVehicles.erase(
                std::remove_if(collisionVehicles.begin(), collisionVehicles.end(),
                               [](const Vehicle& vehicle) { return !vehicle.active.load(); }),
                collisionVehicles.end()
            );
        }

        {
            std::lock_guard<std::mutex> lock(vehiclesMutex);
            vehicles.erase(
                std::remove_if(vehicles.begin(), vehicles.end(),
                               [](const Vehicle& vehicle) { return !vehicle.active.load(); }),
                vehicles.end()
            );
        }
    }
}

// Funkcja wątek - dodająca pojazd kolizyjny 
void addCollisionVehicle() {
    while(running){
        // Losujemy czas pojawienia się nowego pojazdu
        int randomDelay = rand() % 3000 + 3000; // Losowy czas od 3 do 5 sekundy
        std::this_thread::sleep_for(std::chrono::milliseconds(randomDelay));
        
        // Losujemy typ pojazdu, kolor oraz prędkość
        char symbol = 'A' + rand() % 26;    // Losowy symbol od A do Z
        int color = rand() % 7 + 1;         // Losowy kolor od 1 do 7
        int acceleration = rand() % 3 + 1;  // Losowe przyspieszenie od 1 do 3

        // Tworzymy nowy pojazd kolizyjny i dodajemy go do wektora
        Vehicle newVehicle = {48, 0, symbol, color, 0, acceleration};

        {
            std::lock_guard<std::mutex> lock(collisionVehiclesMutex);
            collisionVehicles.push_back(std::move(newVehicle));
        }
    }    
}

// Funkcja wątek - uruchamianie wątków pojazdow kolizyjnych
void startCollisionVehicleThreads(std::vector<Vehicle>& collisionVehicles) {
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(2)); // Uruchamianie wątków co 2 sekundy
 
        {
            std::lock_guard<std::mutex> lock(collisionVehiclesMutex);
            // Iteracja przez listę pojazdów i uruchamianie wątków dla nowych pojazdów 
            for (auto& vehicle : collisionVehicles) {
                if (running && vehicle.active) {
                    collisionVehicleThreads.emplace_back(moveCollisionVehicle, std::ref(vehicle));
                }
            }
        }
    }
}

int main() {
    initscr();                              // Inicjalizacja ekranu ncurses
    start_color();                          // Włączenie obsługi kolorów
    curs_set(0);                            // Ukrycie kursora
    noecho();                               // Wyłączenie wyświetlania wpisywanych znaków
    keypad(stdscr, TRUE);                   // Włączenie obsługi klawiatury
    srand(time(NULL));                      // Inicjalizacja generatora liczb pseudolosowych
    timeout(100);                           // Ustawienie timeout dla getch na 100 ms

    init_pair(1, COLOR_RED, COLOR_BLACK);       // Kolor czerwony
    init_pair(2, COLOR_GREEN, COLOR_BLACK);     // Kolor zielony
    init_pair(3, COLOR_BLUE, COLOR_BLACK);      // Kolor niebieski
    init_pair(4, COLOR_CYAN, COLOR_BLACK);      // Kolor cyjan
    init_pair(5, COLOR_YELLOW, COLOR_BLACK);    // Kolor żółty 
    init_pair(6, COLOR_MAGENTA, COLOR_BLACK);   // Kolor magenta 
    init_pair(7, COLOR_WHITE, COLOR_BLACK);     // Kolor biały

    vehicles.emplace_back(3, 3, 'A' + rand() % 26, rand() % 7 + 1, rand() % 50 + 50, 0);
    vehicles.emplace_back(3, 3, 'A' + rand() % 26, rand() % 7 + 1, rand() % 50 + 50, 0);
    vehicles.emplace_back(3, 3, 'A' + rand() % 26, rand() % 7 + 1, rand() % 50 + 50, 0);
    vehicles.emplace_back(3, 3, 'A' + rand() % 26, rand() % 7 + 1, rand() % 50 + 50, 0);
    vehicles.emplace_back(3, 3, 'A' + rand() % 26, rand() % 7 + 1, rand() % 50 + 50, 0);
    vehicles.emplace_back(3, 3, 'A' + rand() % 26, rand() % 7 + 1, rand() % 50 + 50, 0);
    vehicles.emplace_back(3, 3, 'A' + rand() % 26, rand() % 7 + 1, rand() % 50 + 50, 0);
    vehicles.emplace_back(3, 3, 'A' + rand() % 26, rand() % 7 + 1, rand() % 50 + 50, 0);
    vehicles.emplace_back(3, 3, 'A' + rand() % 26, rand() % 7 + 1, rand() % 50 + 50, 0);
    vehicles.emplace_back(3, 3, 'A' + rand() % 26, rand() % 7 + 1, rand() % 50 + 50, 0);

    std::thread cleanupThread(manageVehicles);

    std::thread collisionVehicleAddingThread(addCollisionVehicle);
    std::thread collisionVehicleStartingThread(startCollisionVehicleThreads, std::ref(collisionVehicles));

    for (auto& vehicle : vehicles) {
        vehicleThreads.emplace_back(moveVehicle, std::ref(vehicle));
    }

    std::thread displayThread(refreshScreen);       

    while (getch() != ' ') {}
 
    running = false;

    std::cerr << "Joining collision vehicles adding thread" << std::endl;
    collisionVehicleAddingThread.join();
 
    // Po zatrzymaniu samochodów na torze 
    std::cerr << "Stopping vehicle threads" << std::endl;
    for (auto& vehicle : vehicles) {
        vehicle.active = false; // Sygnalizacja zatrzymania
    }
 
    // Oczekiwanie na zakończenie wszystkich wątków
    std::cerr << "Joining vehicle threads" << std::endl;
        for (auto& t : vehicleThreads) {
            std::cerr << "Joining thread..." << std::endl;
            t.join(); // Czekaj na zakończenie wątków torowych
            std::cerr << "Thread joined" << std::endl;
        } 

    std::cerr << "Joining collision vehicle threads" << std::endl;
    for (auto& t : collisionVehicleThreads) {
        std::cerr << "Joining thread..." << std::endl;
        t.join();   // Czekaj na zakończenie wątków kolizyjnych
        std::cerr << "Thread joined" << std::endl;
    }  

    std::cerr << "Joining collision vehicles starting thread" << std::endl;
    collisionVehicleStartingThread.join();

    std::cerr << "Joining cleanup thread" << std::endl;
    cleanupThread.join();

    std::cerr << "Joining display thread" << std::endl;
    displayThread.join(); 

    std::cerr << "Program finished" << std::endl;
    endwin();

    return 0;
}