# GenericStateMachine
Implementiert eine generische Zustandsmachine in c#, deren Zustände und Kommandos für den Zustandswechsel jeweils durch ein eigenen Enumerator definiert werden.
## Features
- einfache Definition der erlaubten Zustände und deren Übergangsfunktionen in Form von StateTransistions
- Event-basierte Schnittstelle
- für jede Zustand registrierbarer Callback-Handler
- temporäre Verrigelung der Zustandsmachine über ein registrierbaren lock-predicate Callback
- Begrenzung der maximal erlaubten Rekursionstiefe
- Validierung der definierten StateTransistions
- optional definierbarer Endzustand, der nur durch ein Rest() verlassen werden kann
- optional definierbarer Failsafe-Zustand, der aus dem Fehlerzustand erreichbar ist
- optional definierbarer Fehlerzustand, der aus jedem Zustand (außer dem Endzustand) durch 'SetErrorState()' erreicht werden kann
- Fehlerquittierungsfunktion 'ResetErrorWithPreviousState()' mit der aus dem Fehlerzustand wieder in den letzten gültigen Zustand gewechselt werden kann
- Fehlerquittierungsfunktion 'ResetErrorWithFailsafeState()' mit der aus dem Fehlerzustand den Failsafe Zustand gewechselt werden kann
- Fehlerquittierungsfunktion 'ResetErrorWithNewState()' mit der aus dem Fehlerzustand in einen beliebigen gültigen Zustand gewechselt werden kann
- Rücksetzfunktion 'Reset()' mit der die StateMachine in den Anfangszustand zurückgesetzt werden kann
- ### Stellt folgende Events/Callbacks zur Verfügung:
  - StateReachedCallback
    - wird beim Wechesl in einen spezifischen Zustand aufgerufen, sofern für diesen Zustand zuvor ein Callback registriert wurde
  - StateLeavedCallback
    - wird beim Verlassen eines spezifischen Zustands aufgerufen, sofern für diesen Zustand zuvor ein Callback registriert wurde
  - StateChangedEvent
    - wird bei einem Zustandswechsel ausgelöst
  - ErrorBeforeStateChangedEvent
    - wird vor dem Wechsel in den Fehlerzustand ausgelöst
  - ErrorAfterStateChangedEvent
    - wird nach dem Wechsel in den Fehlerzustand ausgelöst
  - ExitBeforeStateChangedEvent
    - wird vor dem Wechsel in den Endzustand ausgelöst
  - ExitAfterStateChangedEvent
    - wird nach dem Wechsel in den Endzustand ausgelöst
  
## Klassen
### StateTransition<T_State, T_TransitionCommand>
  - kapselt einen Ausgangszustand und ein Kommando, mit dem der Ausgangszustand verlassen werden kann
  - T_STATE und T_TransitionCommand müssen vom Typ System.Enum sein
  
### StateTransitionManager<T_State, T_TransitionCommand>
  
### StateMachine<T_State, T_TransitionCommand>
  
### SpecialStates<T_State>
  - Hilfsstruktur für reduzierte StateMachine Konstruktor-Argumente
