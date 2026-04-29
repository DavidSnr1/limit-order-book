# Limit Order Book ? MVP Plan

## Reihenfolge der Implementierung

```
types.h ? order.h ? order_book.h ? addOrder ? Matching ? cancelOrder
```

Jeder Schritt baut auf dem vorherigen auf. Die Reihenfolge folgt den Abhängigkeiten.

---

## 1. `types.h` ? 30 Min

**Aufgabe:** Gemeinsame Grundtypen definieren die überall benutzt werden. Einmal definiert, nie wieder drüber nachdenken.

**Was reinkommt:**

Typen-Aliase die Code lesbar machen:
- `OrderID ? uint64_t` ? eindeutige ID pro Order
- `Price   ? uint64_t` ? in Cent, keine Floats wegen Rundungsfehlern
- `Volume  ? uint32_t` ? Stückzahl

Zwei Enums:
- `Side:        Buy oder Sell`
- `MessageType: Add, Cancel, Execute`

**Warum eigene Datei:** Wenn `order.h`, `order_book.h` und `matching_engine.h` alle dieselben Typen brauchen, müssen sie alle nur `types.h` includen statt voneinander abhängig zu sein.

---

## 2. `order.h` ? 30 Min

**Aufgabe:** Das zentrale Datenobjekt. Jede Order die ins System kommt wird als dieses Struct dargestellt.

**Was reinkommt:**

```cpp
struct Order {
    OrderID  id;         // eindeutige Kennung
    Price    price;      // in Cent
    Volume   volume;     // wie viele Stück
    Side     side;       // Buy oder Sell
    uint64_t timestamp;  // Nanosekunden ? für Time Priority
};
```

**Warum eigene Datei:** Order wird von `order_book.h`, `matching_engine.h` und `feed_simulator.cpp` alle gebraucht. Klare Trennung ? das Objekt selbst hat keine Logik, nur Daten.

---

## 3. `order_book.h` ? 1h

**Aufgabe:** Definiert die Schnittstelle des Order Books ? was es kann, nicht wie es das macht. Die `.h` Datei ist der Vertrag, die `.cpp` Datei ist die Umsetzung.

**Was reinkommt:**

Zwei Datenstrukturen für die zwei Seiten:
- `bids: map<Price, queue<Order>, absteigend>`
- `asks: map<Price, queue<Order>, aufsteigend>`

Ein Index für schnelle Cancels:
- `order_index: unordered_map<OrderID, Position im Buch>`

Drei Methoden:
- `add_order(order)` ? Order einfügen
- `cancel_order(id)` ? Order entfernen
- `display(n)` ? Top N Levels ausgeben

**Warum eigene Datei:** Header-Datei trennt Interface von Implementierung. Andere Dateien die das Order Book benutzen müssen nur den Header kennen ? nicht die interne Logik.

---

## 4. `addOrder` implementieren ? 2h

**Aufgabe:** In `order_book.cpp` ? die tatsächliche Logik hinter `add_order()`.

**Was passiert konzeptionell:**

```
Order kommt rein
      ?
Ist es eine Buy Order?
? In bids[price] Queue hinten einfügen
? In order_index eintragen

Ist es eine Sell Order?
? In asks[price] Queue hinten einfügen
? In order_index eintragen
      ?
Nach jedem Add: Matching Engine aufrufen
? Gibt es jetzt einen Match?
```

**Warum 2h:** Das klingt simpel aber du musst verstehen wie `std::map` und `std::queue` zusammenspielen. Wenn ein Preisniveau zum ersten Mal auftaucht muss die Map einen neuen Eintrag anlegen. Wenn es schon existiert wird die Queue hinten erweitert.

---

## 5. Matching Engine ? 3h

**Aufgabe:** Nach jedem Add prüfen ob ein Trade möglich ist und ihn ausführen.

**Was passiert konzeptionell:**

```
Schau auf besten Bid (höchster Kaufpreis)
Schau auf besten Ask (niedrigster Verkaufspreis)
      ?
Falls best_bid >= best_ask ? Match möglich
      ?
Trade Preis bestimmen:
? Preis der Order die zuerst da war (Passive Side)

Mengen verrechnen:
? buy.volume = 10, sell.volume = 7
? 7 Stück werden gehandelt
? Buy Order hat noch 3 übrig ? bleibt im Buch
? Sell Order ist komplett gefüllt ? wird entfernt
      ?
Nochmal prüfen ? vielleicht gibt es noch einen Match
? Wiederholen bis kein Match mehr möglich
```

**Warum 3h und eigene Datei:** Das ist die komplexeste Logik. Teilfüllungen, mehrfache Matches hintereinander, Preis-Time Priority ? das muss alles korrekt sein. Eigene Datei weil Matching-Logik unabhängig von Order Book Verwaltung ist.

---

## 6. `cancelOrder` ? 1h

**Aufgabe:** Eine bestehende Order aus dem Buch entfernen ? in O(1).

**Was passiert konzeptionell:**

```
OrderID kommt rein
      ?
order_index[id] nachschlagen
? bekomme: Preis und Seite der Order
      ?
In bids[preis] oder asks[preis] Queue
? Order finden und entfernen
      ?
order_index[id] löschen
      ?
Falls Queue jetzt leer:
? Preisniveau komplett aus der Map entfernen
```

**Warum nur 1h:** Der `order_index` macht den schwierigen Teil einfach. Ohne Index müsstest du die gesamte Map durchsuchen um die Order zu finden ? O(n). Mit Index weißt du sofort wo sie liegt ? O(1).

---

## 7. `feed_simulator.cpp` ? 2h

**Aufgabe:** Generiert realistische Test-Orders damit du dein Order Book füttern kannst ohne echte ITCH Daten.

**Was passiert konzeptionell:**

```
Startpreis festlegen: $67.000
      ?
Loop über N Orders:
? Preis = letzter Preis ± kleiner Zufallswert (Random Walk)
? Seite: 50% Buy, 50% Sell (Zufallszahl)
? Menge: meist klein, selten groß
? Typ: 70% Add, 20% Cancel, 10% Market Order
      ?
Order als Struct ausgeben ? direkt an Order Book
```

**Warum eigene Datei:** Simulator ist kein Teil der Engine ? er ist ein Testwerkzeug. Später ersetzt du ihn durch den echten ITCH Parser ohne die Engine anzufassen.

---

## 8. `tests/test_matching.cpp` ? 2h

**Aufgabe:** Sicherstellen dass die Kernlogik korrekt funktioniert.

**Add Tests:**
- Add Buy Order ? ist sie im Buch?
- Add zwei Buy Orders gleicher Preis ? richtige Reihenfolge?
- Add Buy + passenden Sell ? Trade passiert?

**Cancel Tests:**
- Add Order ? Cancel ? ist sie weg?
- Cancel nicht-existierende ID ? kein Crash?

**Matching Tests:**
- Price-Time Priority: zwei Buys gleicher Preis ? ältere Order wird zuerst gefüllt?
- Teilfüllung: Buy 10, Sell 7 ? Buy hat noch 3 übrig im Buch?
- Mehrfacher Match: Buy 10, drei Sells à 4 ? alle drei Sells werden genutzt?

**Warum wichtig:** Ohne Tests weißt du nicht ob Price-Time Priority wirklich stimmt. Ein Order Book das falsch matched ist wertlos.

---

## 9. `README` + `Makefile` ? 1h

**README ? was reinkommt:**
- Ein Satz was das Projekt ist
- Warum Order Books wichtig sind (HFT Kontext)
- Wie man kompiliert und ausführt
- Beispiel Output (L1/L2 Snapshot)
- Was noch kommt (Memory Pool, ITCH Parser)

**Makefile ? was reinkommt:**

```makefile
CXX = g++
CXXFLAGS = -O2 -std=c++17

all:
	$(CXX) $(CXXFLAGS) -o lob main.cpp src/order_book.cpp

test:
	$(CXX) $(CXXFLAGS) -o test tests/test_matching.cpp

clean:
	rm -f lob test
```