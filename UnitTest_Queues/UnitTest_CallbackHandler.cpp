#include "pch.h"
#include <algorithm>
#include <numeric>
#include <vector>
#include <array>
#include <memory>
#include <thread>
#include <future>
#include <atomic>
#include "CppUnitTest.h"
#include "CallbackHandler.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace std::chrono_literals;
using namespace std::chrono;

namespace tiel::concurrent
{
	TEST_CLASS(Test_CallbackHandler)
	{
	public:

		///----------------------------------------------------------------------------------------------
		TEST_METHOD(Test_Interface)
		{
			CallbackHandler<int, std::string> callbackMgr;

			int value1 = -1;
			int value2 = -1;
			int value3 = -1;

			auto callback1 = [&](int i, const std::string& /*s*/)
			{
				value1 = i;
			};
			auto callback2 = [&](int i, const std::string& /*s*/)
			{
				value2 = i;
			};
			auto callback3 = [&](int i, std::string /*s*/)
			{
				value3 = i;
			};

			Assert::IsFalse(callbackMgr.IsCallbackHandleValid(-1), L"Ungültiges Callback-Handle darf nicht gültig sein");
			Assert::IsFalse(callbackMgr.IsCallbackHandleValid(0), L"Callback-Handle darf nicht gültig sein");

			auto handle1 = callbackMgr.AddCallback(callback1);
			callbackMgr.CallAll(handle1, "CallbackHandler");
			Assert::IsTrue(value1 != -1, L"callback1 nicht aufgerufen");		value1 = -1;
			Assert::IsFalse(value2 != -1, L"callback2 unerwartet aufgerufen"); value2 = -1;
			Assert::IsFalse(value3 != -1, L"callback3 unerwartet aufgerufen"); value3 = -1;
			
			auto handle2 = callbackMgr.AddCallback(callback2);
			callbackMgr.CallAll(handle2, "CallbackHandler");
			Assert::IsTrue(value1 != -1, L"callback1 nicht aufgerufen");		value1 = -1;
			Assert::IsTrue(value2 != -1, L"callback2 nicht aufgerufen");		value2 = -1;
			Assert::IsFalse(value3 != -1, L"callback3 unerwartet aufgerufen"); value3 = -1;

			Assert::IsTrue(callbackMgr.IsCallbackHandleValid(handle1), L"Callback-Handle muss gültig sein");
			callbackMgr.RemoveCallback(handle1);
			Assert::IsFalse(callbackMgr.IsCallbackHandleValid(handle1), L"Callback-Handle darf nicht mehr gültig sein");
			callbackMgr.CallAll(handle2, "CallbackHandler");
			Assert::IsFalse(value1 != -1, L"callback1 unerwartet aufgerufen"); value1 = -1;
			Assert::IsTrue(value2 != -1, L"callback2 nicht aufgerufen");		value2 = -1;
			Assert::IsFalse(value3 != -1, L"callback3 unerwartet aufgerufen"); value3 = -1;

			handle1 = callbackMgr.AddCallback(callback1);
			auto handle3 = callbackMgr.AddCallback(callback3);
			callbackMgr.CallAll(handle3, "CallbackHandler");
			Assert::IsTrue(value1 != -1, L"callback1 nicht aufgerufen");		value1 = -1;
			Assert::IsTrue(value2 != -1, L"callback2 nicht aufgerufen");		value2 = -1;
			Assert::IsTrue(value3 != -1, L"callback3 nicht aufgerufen");		value3 = -1;

			callbackMgr.RemoveCallback(handle2);
			callbackMgr.CallAll(handle3, "CallbackHandler");
			Assert::IsTrue(value1 != -1, L"callback1 nicht aufgerufen");		value1 = -1;
			Assert::IsFalse(value2 != -1, L"callback2 unerwartet aufgerufen"); value2 = -1;
			Assert::IsTrue(value3 != -1, L"callback3 nicht aufgerufen");		value3 = -1;

			callbackMgr.RemoveAllCallbacks();
			Assert::IsFalse(value1 != -1, L"callback1 unerwartet aufgerufen"); value1 = -1;
			Assert::IsFalse(value2 != -1, L"callback2 unerwartet aufgerufen"); value2 = -1;
			Assert::IsFalse(value3 != -1, L"callback3 unerwartet aufgerufen"); value3 = -1;

			handle1 = callbackMgr.AddCallback(callback1);
			handle2 = callbackMgr.AddCallback(callback2);
			handle3 = callbackMgr.AddCallback(callback3);
			// Move-Konstruktor testen
			CallbackHandler<int, std::string> callMgr2 = std::move(callbackMgr);
			Assert::IsTrue(callbackMgr.Size() == 0UL, L"Fehler im Move-Konstruktor");
			Assert::IsTrue(callMgr2.Size() == 3UL, L"Fehler im Move-Konstruktor");

			// Move-Zuweisung testen
			CallbackHandler<int, std::string> callMgr3;
			callMgr3 = std::move(callMgr2);
			Assert::IsTrue(callMgr2.Size() == 0UL, L"Fehler bei der  Move-Zuweisung");
			Assert::IsTrue(callMgr3.Size() == 3UL, L"cFehler bei der  Move-Zuweisung");
		}
		///----------------------------------------------------------------------------------------------
		TEST_METHOD(Test_AsyncInterface)
		{
			// Move-Zuweisung testen
			CallbackHandler<int, std::string> cbMgr;
			auto waitCallback = [](int waitMs, const std::string& /*s*/)
									{
										std::this_thread::sleep_for(milliseconds(waitMs));
									};
			auto wait10ms = [](int , const std::string& )
			{
				std::this_thread::sleep_for(milliseconds(10));
			};
			auto wait100ms = [](int, const std::string&)
			{
				std::this_thread::sleep_for(milliseconds(100));
			};
			auto wait500ms = [](int, const std::string&)
			{
				std::this_thread::sleep_for(milliseconds(500));
			};
			auto wait10msException = [](int, const std::string&)
			{
				std::this_thread::sleep_for(milliseconds(10));
				throw std::exception("Test-Exception in Callback");
			};
			auto handle1 = cbMgr.AddCallback(wait10ms);
			auto handle2 = cbMgr.AddCallback(wait10ms);
			auto handle3 = cbMgr.AddCallback(waitCallback);

			// Test: IsCallbackPending()
			Assert::IsTrue(cbMgr.CallAllAsync(50, "TestAsync1"), L"TestAsync1: CallAllAsync() muss erfolgreich sein");
			Assert::IsTrue(cbMgr.CallAllAsync(100, "TestAsync2"), L"TestAsync2: CallAllAsync() darf wegen pending-call nicht true zurückgeben");
			Assert::IsTrue(cbMgr.IsCallbackPending(handle3), L"TestAsync1: Callback kann noch micht fertig sein");
			std::this_thread::sleep_for(milliseconds(100));
			Assert::IsFalse(cbMgr.IsCallbackPending(handle3), L"TestAsync1: Callback muss jetzt fertig sein");
			// Test: auf bestimmte asynchrone Callback warten
			auto handle4 = cbMgr.AddCallback(wait100ms);
			Assert::IsTrue(cbMgr.CallAllAsync(50, "TestAsync3"), L"TestAsync3: CallAllAsync() muss erfolgreich sein");
			Assert::IsTrue(cbMgr.WaitForAsyncCallbackFinished(handle1, true, 100), L"TestAsync3: Timeout darf nicht auftreten");
			Assert::IsFalse(cbMgr.WaitForAsyncCallbackFinished(handle4, true, 10), L"TestAsync3 Timeout erwartet");
			Assert::IsTrue(cbMgr.WaitForAsyncCallbackFinished(handle4, true), L"TestAsync3: kein Timeout erwartet");
			// Test: auf alle asynchronen Callback warten
			cbMgr.RemoveAllCallbacks();
			handle1 = cbMgr.AddCallback(wait100ms);
			handle2 = cbMgr.AddCallback(waitCallback);
			Assert::IsTrue(cbMgr.CallAllAsync(300, "TestAsync4"), L"TestAsync4: CallAllAsync() muss erfolgreich sein");
			Assert::IsFalse(cbMgr.WaitForAsyncCallbacksFinished(true, 50), L"TestAsync4: Timeout erwartet");
			Assert::IsFalse(cbMgr.WaitForAsyncCallbacksFinished(true, 100), L"TestAsync4: Timeout erwartet");
			Assert::IsTrue(cbMgr.WaitForAsyncCallbacksFinished(true, 200), L"TestAsync4: kein Timeout erwartet");
			// Test: Exception in Callback testen
			cbMgr.RemoveAllCallbacks();
			handle1 = cbMgr.AddCallback(wait10ms);
			handle2 = cbMgr.AddCallback(wait10msException);
			Assert::IsTrue(cbMgr.CallAllAsync(50, "TestAsync5"), L"TestAsync5: CallAllAsync() muss erfolgreich sein");
			Assert::IsFalse(cbMgr.WaitForAsyncCallbacksFinished(true, 100), L"TestAsync5: Exception muss false zurückgeben");

			Assert::IsTrue(cbMgr.CallAllAsync(50, "TestAsync6"), L"TestAsync6: CallAllAsync() muss erfolgreich sein");
			Assert::IsFalse(cbMgr.WaitForAsyncCallbackFinished(handle2, true, 100), L"TestAsync6: Exception muss false zurückgeben");

			// ohne Exception-Behandlung
			Assert::IsFalse(cbMgr.CallAllNoExcept(50, "TestAsync7"), L"TestAsync7: CallAllNoExcept() muss false zurückgeben");
			auto throwTest1 = [&]()
			{
				cbMgr.CallAll(50, "TestAsync8");
			};
			Assert::ExpectException<std::exception>(throwTest1, L"TestAsync8: Exception erwartet");

			auto throwTest2 = [&]()
			{
				cbMgr.WaitForAsyncCallbacksFinished(false, 100);
			};
			Assert::IsTrue(cbMgr.CallAllAsync(50, "TestAsync9"), L"TestAsync9: CallAllAsync() muss erfolgreich sein");
			Assert::ExpectException<std::exception>(throwTest2, L"TestAsync9: Exception erwartet");

			Assert::IsTrue(cbMgr.CallAllAsync(50, "TestAsync10"), L"TestAsync10: CallAllAsync() muss erfolgreich sein");
			auto throwTest3 = [&]()
			{
				cbMgr.WaitForAsyncCallbackFinished(handle2, false, 100);
			};
			Assert::ExpectException<std::exception>(throwTest3, L"TestAsync10: Exception erwartet");
		}
		///----------------------------------------------------------------------------------------------
		TEST_METHOD(ThreadSafty)
		{
			std::atomic_size_t	counter = 0;
			std::atomic_int		lastHandle = -1;

			CallbackHandler<int, std::string> handler1;

			// Callback aus unabhängigen Thread aufrufen
			auto future1 = std::async(std::launch::async,
				[&]()
				{
					// warten bis der erste Callback hinzugefügt wurde
					while(handler1.Size() == 0)
					{;}
					for(int i = 0; i < 100000; i++)
					{
						handler1.CallAll(i, "TestAsync");
					}
				});
			// Callbacks in unabhängigen Thread immer wieder hinzufügen und entfernen
			auto future2 = std::async(std::launch::async,
				[&]()
				{
					for(int j = 0; j < 10000; j++)
					{
						for(int i = 0; i < 10; i++)
						{
							lastHandle = handler1.AddCallback(
								[&](int i, std::string s)
								{
									Assert::IsTrue(i >= 0, L"'i' ungültig");
									Assert::IsFalse(s.empty(), L"'s' ungültig");
									++counter;
								});
						}
						for(int i = 0; i < 10; i++)
						{
							handler1.RemoveCallback(i);
						}
					}
				});
			future1.wait();
			future2.wait();
			Assert::IsTrue(counter > 0);
		}
	};
}
