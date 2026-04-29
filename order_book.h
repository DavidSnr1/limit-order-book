#pragma once;
#include "order.h";

//eine buy side list an orders absteigend im preis

//eine sell side list an orders aufsteigend im preis

//eine hashmap mit order id als key und listen pos in buy/sell liste als value

//Methoden:

// add_order: einfügen in buy/sell liste, hasmap updaten, match check durchführen

// cancel_order: index aus hashmap nutzen um aus buy/sell liste entfernen, hasmap updaten

// display: obersten n orders von buy/sell liste anzeigen