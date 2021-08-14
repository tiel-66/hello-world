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
	TEST_CLASS(Test_LockFreeQueue)
	{
	public:
		///-------------------------------------------------------------------------------------------
		TEST_METHOD(TestInterface)
		{
			constexpr size_t NUM_PUSHES = 8;
			constexpr int64_t SUM_TOTAL = static_cast<int64_t>(NUM_PUSHES*(NUM_PUSHES+1.0)/2.0);

			{
				LockFreeQueue<int64_t>	queue(NUM_PUSHES);
				std::vector<int64_t>	pops(NUM_PUSHES, 0);

				// füllen->zurücksetzen->füllen->schließen-lesen
				Assert::IsTrue(queue.IsEmpty(), L"Queue muss leer sein");
				for(size_t i = 1; i <= queue.MaxSize(); i++)
				{
					Assert::IsTrue(queue.TryPush(i), L"TryPush(): unerwartet fehlgeschlagen");
				}
				Assert::IsTrue(queue.IsFull(), L"Queue muss voll sein");
				Assert::AreEqual(queue.MaxSize(), queue.Size(), L"Queue muss voll sein");
				Assert::IsFalse(queue.TryPush(1234), L"TryPush() in volle Queue darf nicht erfolgreich sein");
				Assert::IsTrue(queue.IsFull(), L"Queue muss voll sein");
				Assert::AreEqual(queue.MaxSize(), queue.Size(), L"Queue muss voll sein");

				queue.Reset();
				Assert::IsTrue(queue.IsEmpty(), L"Queue muss leer sein");
				for(size_t i = 1; i < queue.MaxSize(); i++)
				{
					Assert::IsTrue(queue.TryPush(i), L"TryPush(): unerwartet fehlgeschlagen");
				}
				Assert::IsFalse(queue.IsFull(), L"Queue darf nicht voll sein");
				Assert::IsTrue(queue.TryPush(queue.MaxSize()), L"TryPush(): unerwartet fehlgeschlagen");
				Assert::IsTrue(queue.IsFull(), L"Queue muss voll sein");

				queue.Close();
				Assert::IsTrue(queue.IsClosed(), L"Queue muss geschlossen sein");
				Assert::IsTrue(queue.IsFull(), L"Queue muss weiterhin voll sein");

				for(size_t i = 0; i < NUM_PUSHES; i++)
				{
					pops[i] = queue.TryPop().value_or(0);
				}
				int64_t	sumAllResults = std::accumulate(pops.begin(), pops.end(), 0LL);
				Assert::AreEqual<size_t>(sumAllResults, SUM_TOTAL, L"unerwartete Summe aller empfangener Werte"); // <size_t> für VS2017 erforderlich
			}
			//  Move-Konstruktor Move-Zuweisung
			{
				LockFreeQueue<int64_t>	queue1(NUM_PUSHES);
				std::vector<int64_t>	pops(NUM_PUSHES, 0);

				for(size_t i = 1; i <= queue1.MaxSize(); i++)
				{
					Assert::IsTrue(queue1.TryPush(i), L"TryPush(): unerwartet fehlgeschlagen");
				}
				Assert::IsTrue(queue1.IsFull(), L"Queue1 muss voll sein");

				LockFreeQueue<int64_t>	queue2 = std::move(queue1);
				Assert::IsTrue(queue1.IsEmpty(), L"Queue1 muss nach Move-Konstruktor leer sein");
				Assert::IsTrue(queue2.IsFull(), L"Queue2 muss voll sein");

				queue1 = std::move(queue2);
				Assert::IsTrue(queue2.IsEmpty(), L"Queue2 muss nach Move-Zuweisung leer sein");
				Assert::IsTrue(queue1.IsFull(), L"Queue1 muss voll sein");

				for(size_t i = 0; i < NUM_PUSHES; i++)
				{
					pops[i] = queue1.TryPop().value_or(0);
				}
				int64_t	sumAllResults = std::accumulate(pops.begin(), pops.end(), 0LL);
				Assert::AreEqual<size_t>(sumAllResults, SUM_TOTAL, L"unerwartete Summe aller empfangener Werte"); // <size_t> für VS2017 erforderlich
			}

		}
		///----------------------------------------------------------------------------------------------
		/// Reset-Funktion testen
		TEST_METHOD(RemoveByFilter)
		{
			int					FILTER_VALUE = 42;
			LockFreeQueue<int>	queue;
			auto				filterElements = [filterValue = FILTER_VALUE](const int& value)->bool { return (value == filterValue); };
			auto				filterAllElements = [filterValue = FILTER_VALUE](const int&)->bool {return true; };

			Assert::IsTrue(queue.IsEmpty(), L"Queue muss leer sein");
			Assert::IsTrue(queue.TryPush(42), L"Push() muss erfolgreich sein");
			Assert::IsTrue(queue.TryPush(1), L"Push() muss erfolgreich sein");
			Assert::IsTrue(queue.TryPush(42), L"Push() muss erfolgreich sein");
			Assert::IsTrue(queue.TryPush(2), L"Push() muss erfolgreich sein");
			Assert::IsTrue(queue.TryPush(3), L"Push() muss erfolgreich sein");
			Assert::IsTrue(queue.TryPush(42), L"Push() muss erfolgreich sein");
			Assert::IsFalse(queue.IsEmpty(), L"Queue darf nicht leer sein");
			Assert::IsTrue(queue.RemoveByFilter(filterElements), L"es muessen Elemente herausgefilter worden sein");
			Assert::AreEqual<size_t>(3ULL, queue.Size(), L"unerwartete Anzahl verbleibender Elemente"); // <size_t> für VS2017 erforderlich
			Assert::IsFalse(queue.IsEmpty(), L"Queue darf nicht leer sein");
			Assert::IsFalse(queue.RemoveByFilter(filterElements), L"es duefen keine weiteren Elemente herausgefilter worden sein");
			Assert::IsTrue(queue.RemoveByFilter(filterAllElements), L"alle uebrigen Elemente muessen herausgefilter worden sein");
			Assert::IsTrue(queue.IsEmpty(), L"Queue muss jetzt leer sein");
		}
		///-------------------------------------------------------------------------------------------
		TEST_METHOD(WithIntegralType)
		{
			constexpr size_t	QUEUE_SIZE = 3;
			LockFreeQueue<int> queue(QUEUE_SIZE);

			Assert::IsTrue(queue.IsEmpty(), L"Queue muss zu beginn leer sein");
			for(int i = 0; i < static_cast<int>(QUEUE_SIZE); i++)
			{
				Assert::IsTrue(queue.TryPush(i), L"TryPush(): unerwartet fehlgeschlagen");
			}
			Assert::IsTrue(queue.IsFull(), L"Queue muss voll sein");
			Assert::IsFalse(queue.TryPush(42), L"TryPush() muss bei voller Queue FALSE zurückgeben");
			Assert::IsTrue(queue.IsFull(), L"Queue muss voll sein");
			for(int i = 0; i < static_cast<int>(QUEUE_SIZE); i++)
			{
				auto opti = queue.TryPop();
				Assert::IsTrue(opti.has_value(), L"TryPop() muss gültiges std::optional<T> liefern");
				Assert::AreEqual<int>(opti.value(), i, L"TryPop(): unerwarteter Wert");
			}
			Assert::IsTrue(queue.IsEmpty(), L"Queue muss zu beginn leer sein");
			auto opti = queue.TryPop();
			Assert::IsFalse(opti.has_value(), L"TryPop() muss auf leere Queue ungültiges std::optional<T> liefern");
			Assert::IsTrue(queue.IsEmpty(), L"Queue muss leer sein");

		}
		///----------------------------------------------------------------------------------------------
		TEST_METHOD(WithMoveOnlyType)
		{
			constexpr size_t	QUEUE_SIZE = 3;
			LockFreeQueue<std::unique_ptr<int>> queue(QUEUE_SIZE);

			Assert::IsTrue(queue.IsEmpty(), L"Queue muss zu beginn leer sein");
			for(int i = 0; i < static_cast<int>(QUEUE_SIZE); i++)
			{
				Assert::IsTrue(queue.TryPush(std::make_unique<int>(i)), L"TryPush() : unerwartet fehlgeschlagen");
			}
			Assert::IsTrue(queue.IsFull(), L"Queue muss voll sein");
			Assert::IsFalse(queue.TryPush(std::make_unique<int>(42)), L"TryPush() muss bei voller Queue FALSE zurückgeben");
			Assert::IsTrue(queue.IsFull(), L"Queue muss voll sein");
			for(int i = 0; i < static_cast<int>(QUEUE_SIZE); i++)
			{
				auto opti = queue.TryPop();
				Assert::IsTrue(opti.has_value(), L"TryPop() muss gültiges std::optional<T> liefern");
				Assert::AreEqual<int>(*opti.value(), i, L"TryPop(): unerwarteter Wert");
			}
			Assert::IsTrue(queue.IsEmpty(), L"Queue muss zu beginn leer sein");
			auto opti = queue.TryPop();
			Assert::IsFalse(opti.has_value(), L"TryPop() muss auf leere Queue ungültiges std::optional<T> liefern");
			Assert::IsTrue(queue.IsEmpty(), L"Queue muss leer sein");
		}
		///----------------------------------------------------------------------------------------------
		TEST_METHOD(MultipleConsumer)
		{
			constexpr size_t NUM_CONSUMER = 8;
			constexpr size_t NUM_POPS_PER_CONSUMER = 12500;
			constexpr int64_t SUM_TOTAL = static_cast<int64_t>(NUM_CONSUMER*NUM_POPS_PER_CONSUMER*(NUM_CONSUMER*NUM_POPS_PER_CONSUMER+1.0)/2.0);

			std::vector<std::vector<int64_t>>	popValues(NUM_CONSUMER, std::vector<int64_t>(NUM_POPS_PER_CONSUMER, 0));
			std::vector<std::thread>			threadPool;
			LockFreeQueue<int64_t>				queue(NUM_CONSUMER*NUM_POPS_PER_CONSUMER);

			// Consumer-Threads erzeugen
			for(size_t i = 0; i < NUM_CONSUMER; i++)
			{
				threadPool.emplace_back([&queue, &popValues](size_t index)
					{
						for(auto& result : popValues[index])
						{
							std::optional<int64_t> optValue;
							while(!optValue.has_value())
							{
								optValue = queue.TryPop();
							}
							result = optValue.value_or(0);
						}
					}, i);
			}

			// Producer erzeugt Werte
			for(int64_t i = 1; i <= static_cast<int64_t>(NUM_CONSUMER*NUM_POPS_PER_CONSUMER); ++i)
			{
				while(!queue.TryPush(i))
					;
			}
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
		TEST_METHOD(MultipleProducer)
		{
			constexpr size_t NUM_PRODUCER = 8;
			constexpr size_t NUM_PUSHES_PER_PRODUCER = 12500;
			constexpr int64_t SUM_TOTAL = static_cast<int64_t>(NUM_PRODUCER*NUM_PUSHES_PER_PRODUCER*(NUM_PRODUCER*NUM_PUSHES_PER_PRODUCER+1.0)/2.0);

			std::vector<int64_t>		popValues(NUM_PRODUCER*NUM_PUSHES_PER_PRODUCER, 0);
			std::vector<std::thread>	threadPool;
			LockFreeQueue<int64_t>		queue(NUM_PRODUCER*NUM_PUSHES_PER_PRODUCER);

			// Comsumer-Thread erzeugen
			threadPool.emplace_back([&queue, &popValues]()
				{
					for(auto& result : popValues)
					{
						std::optional<int64_t> optValue;
						while(!optValue.has_value())
						{
							optValue = queue.TryPop();
						}
						result = optValue.value_or(0);
					}
				});

			// Producer-Threads erzeugen
			for(size_t i = 0; i < NUM_PRODUCER; i++)
			{
				const int64_t firstValue = static_cast<int64_t>(i*NUM_PUSHES_PER_PRODUCER+1);
				const int64_t lastValue = static_cast<int64_t>(firstValue+NUM_PUSHES_PER_PRODUCER-1);

				threadPool.emplace_back([&queue](const int64_t firstValue, const int64_t lastValue)
					{
						for(int64_t i = firstValue; i <= lastValue; ++i)
						{
							while(!queue.TryPush(i))
								;
						}
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
		///----------------------------------------------------------------------------------------------
		TEST_METHOD(MultipleConsumerProducer)
		{
			constexpr size_t NUM_CONSUMER_PRODUCER = 4;
			constexpr size_t NUM_PUSHES_PER_PRODUCER = 250000;
			constexpr int64_t SUM_TOTAL = static_cast<int64_t>(NUM_CONSUMER_PRODUCER*NUM_PUSHES_PER_PRODUCER*(NUM_CONSUMER_PRODUCER*NUM_PUSHES_PER_PRODUCER+1.0)/2.0);

			std::vector<std::vector<int64_t>>	popValues(NUM_CONSUMER_PRODUCER, std::vector<int64_t>(NUM_PUSHES_PER_PRODUCER, 0));
			std::vector<std::thread>			threadPoolConsumer;
			std::vector<std::thread>			threadPoolProducer;
			LockFreeQueue<int64_t>				queue(NUM_CONSUMER_PRODUCER*NUM_PUSHES_PER_PRODUCER);


			// Consumer-Threads erzeugen
			for(size_t i = 0; i < NUM_CONSUMER_PRODUCER; i++)
			{
				threadPoolConsumer.emplace_back([&queue, &popValues](size_t index)
					{
						for(auto& result : popValues[index])
						{
							std::optional<int64_t> optValue;
							while(!optValue.has_value())
							{
								optValue = queue.TryPop();
							}
							result = optValue.value_or(0);
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
						for(int64_t i = firstValue; i <= lastValue; ++i)
						{
							while(!queue.TryPush(i))
								;
						}
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
		/// LockFreeQueue nur aus Haupt-Thread
		TEST_METHOD(SingleThread_LockFreeQueue)
		{
			constexpr size_t NUM_PUSHES = 100000;
			constexpr int64_t SUM_TOTAL = static_cast<int64_t>(NUM_PUSHES*(NUM_PUSHES+1.0)/2.0);

			LockFreeQueue<int64_t>	queue;
			std::vector<int64_t>	pops(NUM_PUSHES, 0);

			for(size_t i = 1; i <= NUM_PUSHES; i++)
			{
				Assert::IsTrue(queue.TryPush(i), L"TryPush(): unerwartet fehlgeschlagen");
			}
			for(size_t i = 0; i < NUM_PUSHES; i++)
			{
				pops[i] = queue.TryPop().value_or(0);
			}
			int64_t	sumAllResults = std::accumulate(pops.begin(), pops.end(), 0LL);
			Assert::AreEqual<size_t>(sumAllResults, SUM_TOTAL, L"unerwartete Summe aller empfangener Werte"); // <size_t> für VS2017 erforderlich
		}
		///----------------------------------------------------------------------------------------------
		/// Vergleichszeiten zur unsynchronisierten Queue aus Haupt-Thread
		TEST_METHOD(SingleThread_StdQueue)
		{
			constexpr size_t NUM_PUSHES = 100000;
			constexpr int64_t SUM_TOTAL = static_cast<int64_t>(NUM_PUSHES*(NUM_PUSHES+1.0)/2.0);

			std::queue<int64_t>		queue;
			std::vector<int64_t>	pops(NUM_PUSHES, 0);

			for(size_t i = 1; i <= NUM_PUSHES; i++)
			{
				queue.push(i);
			}
			for(size_t i = 0; i < NUM_PUSHES; i++)
			{
				pops[i] = queue.front();
				queue.pop();
			}
			int64_t	sumAllResults = std::accumulate(pops.begin(), pops.end(), 0LL);
			Assert::AreEqual<size_t>(sumAllResults, SUM_TOTAL, L"unerwartete Summe aller empfangener Werte"); // <size_t> für VS2017 erforderlich
		}
		///----------------------------------------------------------------------------------------------
	};
}
