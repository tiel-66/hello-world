// TestConsole64_.cpp : Diese Datei enthält die Funktion "main". Hier beginnt und endet die Ausführung des Programms.
//

#include <iostream>
#include <atomic>
#include <functional>
#include <format>
#include "ConcurrentQueue.h"
#include "CallbackHandler.h"
#include "SimpleTimer.h"

using namespace std;
using namespace std::string_literals;
using namespace tiel::timer;

using namespace tiel::concurrent::container;
using namespace tiel::concurrent;

//_________________________________________________________________________________________________
CallbackHandler<int, std::string> TestCallbackHandler(CallbackHandler<int, std::string>&& ch)
{
	auto callback1 = [](int i, std::string s)
	{
		cout << std::format("V1: {} = {})\n", s, i);
	};
	auto callback2 = [](int i, std::string s)
	{
		cout << std::format("V2: {} = {})\n", s, i);
	};
	auto callback3 = [](int i, std::string s)
	{
		cout << std::format("V3: {} = {})\n", s, i);
	};
	auto first = ch.AddCallback(callback1);
	ch.CallAll(first, "CallbackHandler");
	cout << "---------------------------\n";
	auto second = ch.AddCallback(callback2);
	ch.CallAll(second, "CallbackHandler");
	cout << "---------------------------\n";
	ch.RemoveCallback(first);
	ch.CallAll(second, "CallbackHandler");
	cout << "---------------------------\n";
	first = ch.AddCallback(callback1);
	auto third = ch.AddCallback(callback3);
	ch.CallAll(third, "CallbackHandler");
	cout << "---------------------------\n";
	ch.RemoveCallback(second);
	ch.CallAll(third, "CallbackHandler");
	cout << "---------------------------\n";
	cout << "alle Callbacks entfernt\n";
	ch.RemoveAllCallbacks();
	ch.CallAll(third, "CallbackHandler");
	cout << "---------------------------\n";

	// Vorbereitung für Move-Operationen
	first = ch.AddCallback(callback1);
	first = ch.AddCallback(callback2);
	first = ch.AddCallback(callback3);
	return std::move(ch);
}
//_________________________________________________________________________________________________
void TestCallbackHandler()
{
	CallbackHandler<int, std::string> handler1;
	CallbackHandler<int, std::string> handler2;

	handler2 = TestCallbackHandler(std::move(handler1));
	cout << "--------- nach Move -------------\n";
	handler2.CallAll(55, "Handler2");
}
//_________________________________________________________________________________________________
void TestBlockingQueue()
{
	using namespace std::chrono_literals;

	BlockingQueue<int> queue;
	bool pushSuccesful = false;

	// pop-Timeout testen
	SimpleTimer tmr;
	auto timeoutValue = queue.Pop(10);
	tmr.dStopMs();
	tmr.PrintTimeMeasurement("Pop-Timeout");

	pushSuccesful = queue.Push(11);
	queue.Reset(true);
	pushSuccesful = queue.Push(1);
	pushSuccesful = queue.Push(2);
	pushSuccesful = queue.Push(3);
	// jetzt aus der Queue holen
	auto val1 = queue.Pop();
	auto val2 = queue.Pop();
	auto val3 = queue.Pop();

	pushSuccesful = queue.Push(1);
	pushSuccesful = queue.Push(2);
	pushSuccesful = queue.Push(3);
	val1 = queue.Pop();
	queue.Close();
	val2 = queue.Pop();
	val3 = queue.Pop();
	auto val4 = queue.Pop();
	pushSuccesful = queue.Push(4);
	val4 = queue.Pop();
	queue.Reset(true);
}
//_________________________________________________________________________________________________
int main(int argc, char* argv[])
{
	vector<string>	arguments(argv + 1, argv + argc);
	SimpleTimer		tmr;

	TestBlockingQueue();
	TestCallbackHandler();

	tmr.PrintElapsedTime("Verstrichene Zeit");
	return 0;
}

