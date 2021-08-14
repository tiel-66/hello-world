#include "pch.h"
#include <algorithm>
#include <numeric>
#include <vector>
#include <array>
#include <memory>
#include <thread>
#include "CppUnitTest.h"
#include "ConcurrentQueue.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
namespace tiel::concurrent::container
{
	///_______________________________________________________________________________________________
	TEST_CLASS(Test_BlockingQueue)
	{
		struct CopyOnly
		{
			int value = 0;
			CopyOnly(int _value) : value {_value} { }
			CopyOnly(const CopyOnly&) = default;
			CopyOnly& operator=(const CopyOnly&) = default;

			CopyOnly(CopyOnly&&) = delete;
			CopyOnly& operator=(CopyOnly&&) = delete;
			bool operator==(const CopyOnly& rhs) const { return rhs.value == value; }
		};

		///----------------------------------------------------------------------------------------------
		/// Producer (Push) und Consumer (Pop) aus dem Haupt-Thread aufrufen
		TEST_METHOD(TestInterface)
		{
			{
				BlockingQueue<int> queue;

				Assert::IsTrue(queue.IsEmpty(), L"Queue muss leer sein");
				Assert::IsTrue(queue.Push(1), L"Push() muss erfolgreich sein");
				Assert::IsFalse(queue.IsEmpty(), L"Queue darf nicht leer sein");
				Assert::IsTrue(queue.Push(2), L"Push() muss erfolgreich sein");
				Assert::IsTrue(queue.Push(3), L"Push() muss erfolgreich sein");
				// jetzt aus der Queue holen
				auto val1 = queue.Pop();
				auto val2 = queue.Pop();
				auto val3 = queue.Pop();
				Assert::IsTrue(queue.IsEmpty(), L"Queue muss leer sein");

				Assert::IsTrue(queue.Push(1), L"Push() muss erfolgreich sein");
				Assert::IsTrue(queue.Push(2), L"Push() muss erfolgreich sein");
				Assert::IsTrue(queue.Push(3), L"Push() muss erfolgreich sein");
				val1 = queue.Pop();
				Assert::IsTrue(val1.has_value(), L"val1 muss einen Wert haben");
				queue.Close();
				Assert::IsTrue(queue.IsClosed(), L"Queue muss geschlossen sein");
				val2 = queue.Pop();
				Assert::IsTrue(val1.has_value(), L"val2 muss einen Wert haben");
				val3 = queue.Pop();
				Assert::IsTrue(val1.has_value(), L"val3 muss einen Wert haben");
				auto val4 = queue.Pop();
				Assert::IsFalse(val4.has_value(), L"val4 darf keinen Wert haben");
				Assert::IsFalse(queue.Push(4), L"Push() darf bei geschlossener queue nicht true zurück geben");
				Assert::IsTrue(queue.IsEmpty(), L"Queue muss bei geschlossener Queue nach Push leer sein");
				val4 = queue.Pop();
				Assert::IsFalse(val4.has_value(), L"val4 darf keinen Wert haben");
			}
			// Move-Konstruktor und Move-Zuweisung
			{
				BlockingQueue<int> queue1;
				Assert::IsTrue(queue1.Push(1), L"Push() muss erfolgreich sein");
				Assert::IsTrue(queue1.Push(2), L"Push() muss erfolgreich sein");
				Assert::IsTrue(queue1.Push(3), L"Push() muss erfolgreich sein");
				const auto expectedSize = queue1.Size();

				BlockingQueue<int> queue2 = std::move(queue1);
				Assert::IsTrue(queue1.IsEmpty(), L"Queue1 muss leer sein");
				Assert::AreEqual(expectedSize, queue2.Size(), L"Nach Move-Konstruktor muss Queue2 alle Elemente haben");

				queue1 = std::move(queue2);
				Assert::IsTrue(queue2.IsEmpty(), L"Queue2 muss leer sein");
				Assert::AreEqual(expectedSize, queue1.Size(), L"Nach Move-Zuweisung muss Queue1 alle Elemente haben");

				auto val1 = queue1.Pop();
				Assert::AreEqual(1, val1.value_or(0), L"unerwarteter Wert für val1");
				auto val2 = queue1.Pop();
				Assert::AreEqual(2, val2.value_or(0), L"unerwarteter Wert für val1");
				auto val3 = queue1.Pop();
				Assert::AreEqual(3, val3.value_or(0), L"unerwarteter Wert für val1");
			}
		}
		///----------------------------------------------------------------------------------------------
		/// Reset-Funktion testen
		TEST_METHOD(ResetQueue)
		{
			BlockingQueue<int> queue;

			Assert::IsTrue(queue.IsEmpty(), L"Queue muss leer sein");
			Assert::IsTrue(queue.Push(1), L"Push() muss erfolgreich sein");
			Assert::IsTrue(queue.Push(2), L"Push() muss erfolgreich sein");
			Assert::IsTrue(queue.Push(3), L"Push() muss erfolgreich sein");
			Assert::IsFalse(queue.IsEmpty(), L"Queue darf nicht leer sein");
			queue.Reset(true);
			Assert::IsTrue(queue.IsEmpty(), L"Queue muss leer sein");
			Assert::IsFalse(queue.IsClosed(), L"Queue darf nicht geschlossen sein");

			Assert::IsTrue(queue.Push(1), L"Push() muss erfolgreich sein");
			Assert::IsTrue(queue.Push(2), L"Push() muss erfolgreich sein");
			Assert::IsTrue(queue.Push(3), L"Push() muss erfolgreich sein");
			Assert::IsFalse(queue.IsEmpty(), L"Queue darf nicht leer sein");
			queue.Reset(false);
			Assert::IsTrue(queue.IsEmpty(), L"Queue muss leer sein");
			Assert::IsTrue(queue.IsClosed(), L"Queue muss geschlossen sein");
		}
		///----------------------------------------------------------------------------------------------
		/// Reset-Funktion testen
		TEST_METHOD(RemoveByFilter)
		{
			int					FILTER_VALUE = 42;
			BlockingQueue<int>	queue;
			auto				filterElements =	[filterValue = FILTER_VALUE](const int &value)->bool { return (value == filterValue); };
			auto				filterAllElements = [filterValue = FILTER_VALUE](const int&)->bool {return true; };

			Assert::IsTrue(queue.IsEmpty(), L"Queue muss leer sein");
			Assert::IsTrue(queue.Push(42), L"Push() muss erfolgreich sein");
			Assert::IsTrue(queue.Push(1), L"Push() muss erfolgreich sein");
			Assert::IsTrue(queue.Push(42), L"Push() muss erfolgreich sein");
			Assert::IsTrue(queue.Push(2), L"Push() muss erfolgreich sein");
			Assert::IsTrue(queue.Push(3), L"Push() muss erfolgreich sein");
			Assert::IsTrue(queue.Push(42), L"Push() muss erfolgreich sein");
			Assert::IsFalse(queue.IsEmpty(), L"Queue darf nicht leer sein");
			Assert::IsTrue(queue.RemoveByFilter(filterElements), L"es muessen Elemente herausgefilter worden sein");
			Assert::AreEqual<size_t>(3ULL, queue.Size(), L"unerwartete Anzahl verbleibender Elemente"); // <size_t> für VS2017 erforderlich
			Assert::IsFalse(queue.IsEmpty(), L"Queue darf nicht leer sein");
			Assert::IsFalse(queue.RemoveByFilter(filterElements), L"es duefen keine weiteren Elemente herausgefilter worden sein");
			Assert::IsTrue(queue.RemoveByFilter(filterAllElements), L"alle uebrigen Elemente muessen herausgefilter worden sein");
			Assert::IsTrue(queue.IsEmpty(), L"Queue muss jetzt leer sein");
		}
		///----------------------------------------------------------------------------------------------
		/// Queue mit Move-Only-Type (Single-Thread)
		TEST_METHOD(With_MoveOnlyType)
		{
			BlockingQueue<std::unique_ptr<int>> queue;

			Assert::IsTrue(queue.Push(std::make_unique<int>(42)), L"Push() muss klappen");
			auto optOut = queue.Pop();
			Assert::IsTrue(optOut.has_value() && (optOut.value() != nullptr), L"unique_ptr darf nicht nullptr sein");
			std::unique_ptr<int> uniquePtr = *std::move(optOut);
			Assert::IsTrue(optOut.has_value() && !optOut.value(), L"Wert von optOut nach std::move() muss einen leeren unique_ptr<T> halten");
			Assert::IsTrue(queue.Push(std::make_unique<int>(43)), L"Push() muss klappen");
			uniquePtr = *std::move(queue.Pop());
			Assert::IsTrue(uniquePtr != nullptr, L"uniquePtr muss einen Wert halten");

			Assert::IsTrue(queue.Push(std::make_unique<int>(44)), L"Push() muss klappen");
			optOut = queue.Pop(10);
			Assert::IsTrue(optOut.has_value() && (optOut.value() != nullptr), L"pi darf nicht nullptr sein");
		}
		///----------------------------------------------------------------------------------------------
		/// Queue mit Copy-Only-Type (Single-Thread)
		TEST_METHOD(With_NoneMoveableType)
		{
			CopyOnly co{ 42 };
			BlockingQueue<CopyOnly> queue;
			Assert::IsTrue(queue.Push(co), L"Push() muss klappen");
			auto pi = queue.Pop();
			Assert::IsTrue(pi.has_value() && (pi.value() == co), L"pi darf nicht nullptr sein");

			Assert::IsTrue(queue.Push(co), L"Push() muss klappen");
			pi = queue.Pop();
			Assert::IsTrue(pi.has_value() && (pi.value() == co), L"pi darf nicht nullptr sein");

			co.value = 43;
			Assert::IsTrue(queue.Push({ co.value }), L"Push() muss klappen");
			pi = queue.Pop(10);
			Assert::IsTrue(pi.has_value() && (pi.value() == co), L"pi muss einen Wert haben");
			pi = queue.Pop(100);
			Assert::IsFalse(pi.has_value(), L"Timeout: pi darf keinen Wert haben");
		}
		///----------------------------------------------------------------------------------------------
		/// BlockingQueue nur aus Haupt-Thread
		TEST_METHOD(SingleThread_BlockingQueue)
		{
			constexpr size_t NUM_PUSHES = 100000;
			constexpr int64_t SUM_TOTAL = static_cast<int64_t>(NUM_PUSHES*(NUM_PUSHES+1.0)/2.0);

			BlockingQueue<int64_t>	queue;
			std::vector<int64_t>	pops(NUM_PUSHES, 0);

			for(size_t i = 1; i <= NUM_PUSHES; i++)
			{
				Assert::IsTrue(queue.Push(i), L"TryPush(): unerwartet fehlgeschlagen");
			}
			for(size_t i = 0; i < NUM_PUSHES; i++)
			{
				pops[i] = queue.Pop().value_or(0);
			}
			int64_t	sumAllResults = std::accumulate(pops.begin(), pops.end(), 0LL);
			Assert::AreEqual<size_t>(sumAllResults, SUM_TOTAL, L"unerwartete Summe aller empfangener Werte"); // <size_t> für VS2017 erforderlich
		}
		///----------------------------------------------------------------------------------------------
		/// Relativ schlechte Performance, wenn viele Consumer auf Daten warten, die nur von einem
		/// Producer geliefert werden.
		TEST_METHOD(MultipleConsumer)
		{
			constexpr size_t NUM_CONSUMER = 8;
			constexpr size_t NUM_POPS_PER_CONSUMER = 12500;
			constexpr int64_t SUM_TOTAL = static_cast<int64_t>(NUM_CONSUMER*NUM_POPS_PER_CONSUMER*(NUM_CONSUMER*NUM_POPS_PER_CONSUMER+1.0)/2.0);

			std::vector<std::array<int64_t, NUM_POPS_PER_CONSUMER>>	popValues(NUM_CONSUMER);
			std::vector<std::thread>			threadPool;
			BlockingQueue<int64_t>				queue;

			// Consumer-Threads erzeugen
			for(size_t i = 0; i < NUM_CONSUMER; i++)
			{
				popValues[i].fill(0);
				threadPool.emplace_back([&queue, &popValues](size_t index)
					{
						for(auto& result : popValues[index])
						{
							result = queue.Pop().value_or(0);
						}
					}, i);
			}

			// Producer erzeugt Werte
			bool allPushesSuccessful = true;
			for(int64_t i = 1; i <= static_cast<int64_t>(NUM_CONSUMER*NUM_POPS_PER_CONSUMER); ++i)
			{
				allPushesSuccessful &= queue.Push(i);
			}
			Assert::IsTrue(allPushesSuccessful, L"Nicht alle Push-Operationen waren erfolgreich");
			// warten bis alle Consumer fertig sind
			for(auto& consumer : threadPool)
			{
				consumer.join();
			}

			int64_t sumAllResults = 0;

			for(size_t i = 0; i < NUM_CONSUMER; i++)
			{
				sumAllResults += std::accumulate(popValues[i].begin(), popValues[i].end(), 0LL);
			}
			Assert::AreEqual<size_t>(sumAllResults, SUM_TOTAL, L"unerwartete Summe aller empfangener Werte"); // <size_t> für VS2017 erforderlich
		}
		///----------------------------------------------------------------------------------------------
		/// Relativ schlechte Performance, wenn viele Consumer auf Daten warten, die nur von einem
		/// Producer geliefert werden.
		TEST_METHOD(CloseQueue_WithBlockingConsumer)
		{
			constexpr size_t NUM_CONSUMER = 8;
			constexpr size_t NUM_POPS_PER_CONSUMER = 100;
			constexpr int64_t SUM_TOTAL = static_cast<int64_t>(NUM_CONSUMER*NUM_POPS_PER_CONSUMER*(NUM_CONSUMER*NUM_POPS_PER_CONSUMER+1.0)/2.0);

			std::vector<std::array<int64_t, NUM_POPS_PER_CONSUMER>>	popValues(NUM_CONSUMER);
			std::vector<std::thread>			threadPool;
			BlockingQueue<int64_t>				queue;

			// Consumer-Threads erzeugen
			for(size_t i = 0; i < NUM_CONSUMER; i++)
			{
				popValues[i].fill(0);
				threadPool.emplace_back([&queue, &popValues](size_t index)
					{
						for(auto& result : popValues[index])
						{
							result = queue.Pop(100).value_or(0);
						}
					}, i);
			}

			// Producer erzeugt nur eine handvoll Wert
			bool allPushesSuccessful = true;
			for(int64_t i = 1; i <= static_cast<int64_t>(NUM_CONSUMER); ++i)
			{
				allPushesSuccessful &= queue.Push(i);
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			queue.Close();
			Assert::IsTrue(allPushesSuccessful, L"Nicht alle Push-Operationen waren erfolgreich");
			// warten bis alle Consumer fertig sind
			for(auto& consumer : threadPool)
			{
				consumer.join();
			}

			int64_t sumAllResults = 0;

			for(size_t i = 0; i < NUM_CONSUMER; i++)
			{
				sumAllResults += std::accumulate(popValues[i].begin(), popValues[i].end(), 0LL);
			}
			Assert::AreNotEqual<size_t>(sumAllResults, SUM_TOTAL, L"Summe darf wegen Timeout nicht stimmen"); // <size_t> für VS2017 erforderlich
		}
		///----------------------------------------------------------------------------------------------
		TEST_METHOD(MultipleConsumerProducer)
		{
			constexpr size_t NUM_CONSUMER_PRODUCER = 4;
			constexpr size_t NUM_PUSHES_PER_PRODUCER = 250000;
			constexpr int64_t SUM_TOTAL = static_cast<int64_t>(NUM_CONSUMER_PRODUCER*NUM_PUSHES_PER_PRODUCER*(NUM_CONSUMER_PRODUCER*NUM_PUSHES_PER_PRODUCER+1.0)/2.0);

			std::vector<std::array<int64_t, NUM_PUSHES_PER_PRODUCER>>	popValues(NUM_CONSUMER_PRODUCER);
			std::vector<std::thread>			threadPoolConsumer;
			std::vector<std::thread>			threadPoolProducer;
			BlockingQueue<int64_t>				queue;


			// Consumer-Threads erzeugen
			for(size_t i = 0; i < NUM_CONSUMER_PRODUCER; i++)
			{
				threadPoolConsumer.emplace_back([&queue, &popValues](size_t index)
					{
						for(auto& result : popValues[index])
						{
							result = queue.Pop().value_or(0);
						}
					}, i);
			}

			// Producer-Threads erzeugen
			for(size_t i = 0; i < NUM_CONSUMER_PRODUCER; i++)
			{
				const int64_t firstValue = static_cast<int64_t>(i*NUM_PUSHES_PER_PRODUCER+1);
				const int64_t lastValue = static_cast<int64_t>(firstValue+NUM_PUSHES_PER_PRODUCER-1);

				threadPoolProducer.emplace_back([&queue](const int64_t firstValue, const int64_t lastValue)
					{
						bool allPushesSuccessful = true;
						for(int64_t i = firstValue; i <= lastValue; ++i)
						{
							allPushesSuccessful &= queue.Push(i);
						}
						Assert::IsTrue(allPushesSuccessful, L"Nicht alle Push-Operationen waren erfolgreich");
					}, firstValue, lastValue);
			}

			// warten bis alle Producer--Threads fertig sind
			for(auto& producer : threadPoolProducer)
			{
				producer.join();
			}
			// warten bis alle Consumer-Threads fertig sind
			for(auto& consumer : threadPoolConsumer)
			{
				consumer.join();
			}

			int64_t sumAllResults = 0;
			for(size_t i = 0; i < NUM_CONSUMER_PRODUCER; i++)
			{
				sumAllResults += std::accumulate(popValues[i].begin(), popValues[i].end(), 0LL);
			}
			Assert::AreEqual<size_t>(sumAllResults, SUM_TOTAL, L"unerwartete Summe aller empfangener Werte"); // <size_t> für VS2017 erforderlich
		}
		///----------------------------------------------------------------------------------------------
		TEST_METHOD(MultipleProducer)
		{
			constexpr size_t NUM_PRODUCER = 8;
			constexpr size_t NUM_PUSHES_PER_PRODUCER = 12500;
			constexpr int64_t SUM_TOTAL = static_cast<int64_t>(NUM_PRODUCER*NUM_PUSHES_PER_PRODUCER*(NUM_PRODUCER*NUM_PUSHES_PER_PRODUCER+1.0)/2.0);

			std::vector<int64_t>		popValues(NUM_PRODUCER*NUM_PUSHES_PER_PRODUCER, 0);
			std::vector<std::thread>	threadPool;
			BlockingQueue<int64_t>		queue;

			// Comsumer-Thread erzeugen
			threadPool.emplace_back([&queue, &popValues]()
				{
					for(auto& result : popValues)
					{
						result = queue.Pop().value_or(0);
					}
				});

			// Producer-Threads erzeugen
			for(size_t i = 0; i < NUM_PRODUCER; i++)
			{
				const int64_t firstValue = static_cast<int64_t>(i*NUM_PUSHES_PER_PRODUCER+1);
				const int64_t lastValue = static_cast<int64_t>(firstValue+NUM_PUSHES_PER_PRODUCER-1);

				threadPool.emplace_back([&queue](const int64_t firstValue, const int64_t lastValue)
					{
						bool allPushesSuccessful = true;
						for(int64_t i = firstValue; i <= lastValue; ++i)
						{
							allPushesSuccessful &= queue.Push(i);
						}
						Assert::IsTrue(allPushesSuccessful, L"Nicht alle Push-Operationen waren erfolgreich");
					}, firstValue, lastValue);
			}

			// warten bis alle Producer- und der Consumer-Thread fertig sind
			for(auto& worker : threadPool)
			{
				worker.join();
			}

			int64_t	sumAllResults = std::accumulate(popValues.begin(), popValues.end(), 0LL);
			Assert::AreEqual<size_t>(sumAllResults, SUM_TOTAL, L"unerwartete Summe aller empfangener Werte"); // <size_t> für VS2017 erforderlich
		}
	};
}
