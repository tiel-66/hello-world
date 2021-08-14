#pragma once
#include <vector>
#include <optional>
#include <functional>
#include <mutex>
#include <future>
#include <atomic>

namespace tiel::concurrent
{
	//________________________________________________________________________________________________
	/// @brief	Klasse zum threadsicheren Verwalten von Callback-Objekten
	/// @remark	Die zu registrierenden Callbacks müssen die Signatur std::function<void(Args ...)> haben
	//	@param	Args	Callback-Parameter
	template <class ... Args>
	class CallbackHandler final
	{
		///_________________________________________________________________________________________________
		/// Hilfsklasse, die den reingereichten atomic_bool Status im Konstruktor auf true, und am Ende der
		/// der Lebensdauer auf false setzt.
		class AtomicStateGuard final
		{
			std::atomic_bool* pState;
		public:
			AtomicStateGuard(std::atomic_bool& stateRef) : pState(&stateRef)
			{
				stateRef = true;
			}
			~AtomicStateGuard()
			{
				*pState = false;
			}
		};

	public:
		using CallbackType = std::function<void(Args ...)>;

		///----------------------------------------------------------------------------------------------
		/// Standard-Konstruktor
		CallbackHandler() = default;
		///----------------------------------------------------------------------------------------------
		/// Typ ist nicht kopierbar
		CallbackHandler(const CallbackHandler&) = delete;
		CallbackHandler& operator=(const CallbackHandler&) = delete;
		///----------------------------------------------------------------------------------------------
		/// Destruktor
		~CallbackHandler()
		{
			//_ASSERT(false); // not tested
			_ASSERT(!IsPendingOperation() && !IsAnyCallbackPending());
			RemoveAllCallbacks();
		}
		///----------------------------------------------------------------------------------------------
		/// @brief Move-Konstruktor
		/// @param mv_other		CallbackHandler, dessen Callbacks übernommen werden. Dessen Calbback-Liste
		///						ist anschließend leer.
		CallbackHandler(CallbackHandler&& mv_other) noexcept
		{
			//_ASSERT(false); // not tested
			std::lock_guard lock(mMutex);
			mCallbacks			= std::move(mv_other.mCallbacks);
			mCallbackFutures	= std::move(mv_other.mCallbackFutures);

			mIsPendingOperation.store(mv_other.mIsPendingOperation.load(std::memory_order_acquire));
			mv_other.mIsPendingOperation.store(false, std::memory_order_relaxed);
		}
		///----------------------------------------------------------------------------------------------
		/// @brief Move-Zuweisungsoperator
		/// @param mv_rhs		CallbackHandler, dessen Callbacks übernommen werden. Dessen Calbback-Liste
		///						ist anschließend leer.
		CallbackHandler& operator=(CallbackHandler&& mv_rhs) noexcept
		{
			//_ASSERT(false); // not tested
			std::scoped_lock lock(mMutex, mv_rhs.mMutex);
			mCallbacks			= std::move(mv_rhs.mCallbacks);
			mCallbackFutures	= std::move(mv_rhs.mCallbackFutures);

			mIsPendingOperation.store(mv_rhs.mIsPendingOperation.load(std::memory_order_acquire));
			mv_rhs.mIsPendingOperation.store(false, std::memory_order_relaxed);
			return *this;
		}
		///----------------------------------------------------------------------------------------------
		/// @brief	Ruft alle angemeldeten Callbacks mit den übergebenen Argumenten nacheinander auf.
		/// @remark	Exception innerhalb der Callbacks werden nicht behandelt
		/// @param ...args		Variable Argumentenliste, deren Anzahl und Typ mit dem Template-Parametern
		///						von CallbackHandler<... Args> übereinstimmen muss.
		void CallAll(Args... args)
		{
			//_ASSERT(false); // not tested
			using namespace std::chrono_literals;

			std::lock_guard		lock(mMutex);
			const size_t		numAvailableCallbacks = mCallbacks.size();
			AtomicStateGuard	stateGuard(mIsPendingOperation);

			for(size_t i = 0; i < numAvailableCallbacks; i++)
			{
				if(mCallbacks[i].has_value())
				{
					// vorherige asynchrone Callbackl per CallAllAsync() sollten abgeschlossen sein
					_ASSERT(!mCallbackFutures[i].valid() ||	(mCallbackFutures[i].wait_for(0ms) == std::future_status::ready));
					try
					{
						mCallbacks[i].value()(args...);
					}
					catch(const std::exception&)
					{
						throw;
					}
				}
			}
		}
		///----------------------------------------------------------------------------------------------
		/// @brief Ruft alle angemeldeten Callbacks mit den übergebenen Argumenten nacheinander auf.
		/// @remark	Exception innerhalb der Callbacks werden gefangen.
		/// @param ...args		Variable Argumentenliste, deren Anzahl und Typ mit dem Template-Parametern
		///						von CallbackHandler<... Args> übereinstimmen muss.
		/// @return				true, wenn alle registrierten Callbacks erfolgreich aufgerufen wurden.
		///						false, wenn in mindestens einem Callback eine Ausnahme ausgelöst wurde
		bool CallAllNoExcept(Args... args)
		{
			//_ASSERT(false); // not tested
			using namespace std::chrono_literals;

			std::lock_guard		lock(mMutex);
			AtomicStateGuard	stateGuard(mIsPendingOperation);
			const size_t		numAvailableCallbacks = mCallbacks.size();
			bool				success = true;

			for(size_t i = 0; i < numAvailableCallbacks; i++)
			{
				if(mCallbacks[i].has_value())
				{
					// vorherige asynchrone Callback per CallAllAsync() sollten abgeschlossen sein
					_ASSERT(!mCallbackFutures[i].valid() ||	(mCallbackFutures[i].wait_for(0ms) == std::future_status::ready));
					try
					{
						mCallbacks[i].value()(args...);
					}
					catch(const std::exception&)
					{
						success = false;
					}
				}
			}
			return success;
		}
		///----------------------------------------------------------------------------------------------
		/// @brief Ruft jeden angemeldeten Callback mit den übergebenen Argumenten über den ThreadPool auf.
		/// @remark				Wenn ein asynchron aufgerufener Callback noch nicht wieder zurückgekehrt
		///						ist, wird er in diesem Aufruf nicht erneut aufgerufen, da CallAllAsync()
		///						sonst blockieren würde.
		/// @param ...args		Variable Argumentenliste, deren Anzahl und Typ mit dem Template-Parametern
		///						von CallbackHandler<... Args> übereinstimmen muss.
		/// @return				true, wenn alle registrierten Callbacks asynchron aufgerufen wurden.
		///						false, wenn mindestens ein registrierter Callback von einem vorherigen
		///						Aufruf durch CallAllAsync() noch nicht wieder zurückgekehrt ist.
		bool CallAllAsync(Args... args)
		{
			//_ASSERT(false); // not tested
			using namespace std::chrono_literals;

			std::lock_guard lock(mMutex);
			bool			arePendingCalls = false;
			const size_t	numAvailableCallbacks = mCallbacks.size();

			for(size_t i = 0; i < numAvailableCallbacks; i++)
			{
				if(mCallbacks[i].has_value())
				{
					if(		!mCallbackFutures[i].valid()
						||	(mCallbackFutures[i].wait_for(0ms) == std::future_status::ready))
					{
						mCallbackFutures[i] = std::async(std::launch::async, mCallbacks[i].value(), args...);
					}
				}
				else
				{
					arePendingCalls = true;
				}
			}
			return !arePendingCalls;
		}
		///----------------------------------------------------------------------------------------------
		/// @brief	Fügt ein neues aufrufbares Objekt der internen Callback-Liste hinzu.
		/// @remark	Als aufrufbare Objekte können (Member-)Funktionen, Lambdas, std::function<void(...)
		///			und Klassenobjekte von Klassen, die den entsprechenden function call operator(...) 
		///			definieren, verwendet werden.
		/// @param callback		aufrufbares Objekt ()
		/// @return				Handle des hinzugefügten Callbacks, über den "callback" mit RemoveCallback()
		///						wieder entfernt werden kann.
		[[nodiscard]] int AddCallback(CallbackType callback)
		{
			//_ASSERT(false); // not tested
			std::lock_guard lock(mMutex);
			_ASSERT(mCallbackFutures.size() == mCallbacks.size());
			const int numAvailableCallbacks = static_cast<int>(mCallbacks.size());

			for(int i = 0; i < numAvailableCallbacks; i++)
			{
				if(!mCallbacks[i].has_value())
				{
					mCallbacks[i] = callback;
					_ASSERT(!mCallbackFutures[i].valid()); // wieso wurde Future in RemoveCallback() nicht entfernt?
					mCallbackFutures[i] = {};
					return i;
				}
			}
			mCallbacks.emplace_back(callback);
			mCallbackFutures.emplace_back();
			return static_cast<int>(mCallbacks.size()-1);
		}
		///----------------------------------------------------------------------------------------------
		/// @brief	Entfernt das Callback-Objekt mit dem angegebenen Handle aus der internen Callback-Liste
		/// @param handle		Handle des Callback-Objekts, das entfernt werden soll
		/// @return				true, wenn ein Callback-Objekt mit dem angegebenen Handle existierte.
		bool RemoveCallback(int handle)
		{
			//_ASSERT(false); // not tested
			using namespace std::chrono_literals;

			std::lock_guard lock(mMutex);
			_ASSERT(mCallbackFutures.size() == mCallbackFutures.size());

			if((handle < 0) || handle >= static_cast<int>(mCallbacks.size()))
			{
				return false;
			}
			_ASSERT(	!mCallbackFutures[handle].valid() 
					||	mCallbackFutures[handle].wait_for(0ms) == std::future_status::ready);

			if(mCallbackFutures[handle].valid())
			{
				mCallbackFutures[handle].get();
			}
			mCallbacks[handle]			= {};
			mCallbackFutures[handle]	= {};
			return true;
		}
		///----------------------------------------------------------------------------------------------
		/// @brief	Entfernt alle Callback-Objekte aus der internen Callback-Liste
		void RemoveAllCallbacks()
		{
			//_ASSERT(false); // not tested
			using namespace std::chrono_literals;

			std::lock_guard lock(mMutex);
			_ASSERT(std::count_if(mCallbackFutures.begin(), mCallbackFutures.end(),
				[](const std::future<void>& f)
				{
					return f.valid() && (f.wait_for(0ms) != std::future_status::ready);
				}) == 0);
			mCallbacks.clear();
			mCallbackFutures.clear();
		}
		///----------------------------------------------------------------------------------------------
		/// @brief	Gibt die Anzahl der intern verwalteten Callback-Objekte zurück.
		/// @return			Anzahl der intern verwalteten Callback-Objekte.
		[[nodiscard]] size_t Size() const
		{
			std::lock_guard lock(mMutex);
			return mCallbacks.size();
		}
		///----------------------------------------------------------------------------------------------
		/// @brief	Prüft (NICHT blockierend), ob die Schnittstelle für unbestimmte Zeit blockiert ist.
		/// @remark	Gilt für synchrone CallAll()- und WaitForXXX-Methoden, die für umbestimmte Zeit die
		///			gesamte Schnittstelle blockieren können.
		/// @return			true, wenn die Schnittstelle für umbestimmte Zeit blockiert ist.
		[[nodiscard]] bool IsPendingOperation() const
		{
			return mIsPendingOperation.load(std::memory_order_relaxed);
		}
		///----------------------------------------------------------------------------------------------
		/// @brief Prüft, ob das Callback-Handle auf ein gültiges Callback-Objekt verweist.
		/// @param handle	Callback-Handle dessen Gültigkeit überprüft wird
		/// @return			true, wenn das Callback-Handle auf ein gültiges Callback-Objekt verweist
		[[nodiscard]] bool IsCallbackHandleValid(int handle) const
		{
			//_ASSERT(false); // not tested
			std::lock_guard lock(mMutex);
			return (handle >= 0) && (handle < mCallbacks.size()) && (mCallbacks[handle].has_value());
		}
		///----------------------------------------------------------------------------------------------
		/// @brief	Prüft, das Callback-Objekt mit dem angegebenen Handle asynchron aufgerufen und
		///			noch nicht zurückgekehrt ist.
		/// @param handle	Callback-Handle dessen Status überprüft wird
		/// @return			true, wenn ein asynchroner Aufruf des Callback-Objekt noch nicht abgeschlossen ist
		[[nodiscard]] bool IsCallbackPending(int handle) const
		{
			//_ASSERT(false); // not tested
			using namespace std::chrono_literals;

			std::lock_guard lock(mMutex);
			if((handle >= 0) && (handle < mCallbacks.size()) && mCallbacks[handle].has_value())
			{
				return		mCallbackFutures[handle].valid() 
						&&	(mCallbackFutures[handle].wait_for(0ms) != std::future_status::ready);
			}
			return false;
		}
		///----------------------------------------------------------------------------------------------
		/// @brief	Prüft, ob mindestens ein Callback-Objekt asynchron aufgerufen aber noch nicht
		///			zurückgekehrt ist.
		/// @remark	Während hier auf den Abschluss aller asynchron aufgerufenen Callbacks gewartet wird
		///			blockieren alle übrigen Schnittstellenmethoden außer IsPendingOperation().
		/// @return			true, wenn mindestens ein asynchron aufgerufenens Callback-Objekt noch 
		///					nicht zurückgekehrt ist
		[[nodiscard]] bool IsAnyCallbackPending() const
		{
			//_ASSERT(false); // not tested
			using namespace std::chrono_literals;

			std::lock_guard lock(mMutex);
			const size_t	numAvailableCallbacks	= mCallbacks.size();

			for(size_t i = 0; i < numAvailableCallbacks; i++)
			{
				if(mCallbacks[i].has_value())
				{
					if(mCallbackFutures[i].valid() && (mCallbackFutures[i].wait_for(0ms) != std::future_status::ready))
					{
						return true;
					}
				}
			}
			return false;
		}
		///----------------------------------------------------------------------------------------------
		/// @brief	Wartet bis alle registrierten Callbacks, die ggf. mittels CallAllAsync() über den
		///			Threadpool aufgrufen wurde, abgeschlossen sind.
		/// @remark	Während hier auf den Abschluss aller asynchron aufgerufenen Callbacks gewartet wird
		///			blockieren alle übrigen Schnittstellenmethoden außer IsPendingOperation().
		/// @param handleException [in]	true, Callback-Ausnahmen, gefangen werden sollen
		/// @param timeoutMs [in]		Timeout in Millisekunden, bis dieser Aufruf spätestens zurückkert.
		///								-1, wenn kein Timout verwendet werden soll.
		/// @return						true, wenn alle Callbacks innerhalb der vorgegebenen Zeit 
		///								fehlerfrei abgeschlossen wurden, sonst false
		bool WaitForAsyncCallbacksFinished(bool handleException, int timeoutMs = -1)
		{
			//_ASSERT(false); // not tested
			using namespace std::chrono;
			bool isSuccess = true;

			if(timeoutMs > 0)
			{
				auto			startTime	 = high_resolution_clock::now();
				milliseconds	remainingMs { timeoutMs };

				std::lock_guard		lock(mMutex);
				AtomicStateGuard	stateGuard(mIsPendingOperation);
				const size_t		numAvailableCallbacks = mCallbacks.size();

				for(size_t i = 0; i < numAvailableCallbacks; i++)
				{
					if(mCallbacks[i].has_value() && mCallbackFutures[i].valid())
					{
						if((mCallbackFutures[i].wait_for(milliseconds(timeoutMs)) == std::future_status::ready))
						{
							try
							{
								mCallbackFutures[i].get(); // prüfen, ob Exception in Callback aufgetreten ist
							}
							catch(const std::exception&)
							{
								if(!handleException)
								{
									throw;
								}
								isSuccess &= false;
							}
						}
						else
						{
							isSuccess &= false;
						}
						remainingMs -= duration_cast<milliseconds>(high_resolution_clock::now() - startTime);
					}
					if(remainingMs.count() <= 0)
					{
						break;
					}
				}
			}
			else
			{
				std::lock_guard		lock(mMutex);
				AtomicStateGuard	stateGuard(mIsPendingOperation);
				const size_t		numAvailableCallbacks = mCallbacks.size();

				for(size_t i = 0; i < numAvailableCallbacks; i++)
				{
					if(mCallbacks[i].has_value() && mCallbackFutures[i].valid())
					{
						try
						{
							mCallbackFutures[i].get(); // prüfen, ob Exception in Callback aufgetreten ist
						}
						catch(const std::exception&)
						{
							if(!handleException)
							{
								throw;
							}
							isSuccess &= false;
						}
					}
				}
			}
			return isSuccess;
		}
		///----------------------------------------------------------------------------------------------
		/// @brief	Wartet bis alle registrierten Callbacks, die ggf. mittels CallAllAsync() über den
		///			Threadpool aufgrufen wurde, abgeschlossen sind.
		/// @remark	Während hier auf den Abschluss des asynchron aufgerufenen Callbacks gewartet wird
		///			blockieren alle übrigen Schnittstellenmethoden, außer IsPendingOperation().
		/// @param handle [in]			Handle des Callback, auf dessen Abschluss gewartet wird.
		/// @param handleException [in]	true, wenn Callback-Ausnahmen gefangen werden sollen.
		/// @param timeoutMs [in]		Timeout in Millisekunden, bis dieser Aufruf spätestens zurückkert.
		///								-1, wenn kein Timout verwendet werden soll.
		/// @return						true, wenn der angegebene Callback innerhalb der vorgegebenen Zeit 
		///								fehlerfrei abgeschlossen werden konnte
		bool WaitForAsyncCallbackFinished(int handle, bool handleException, int timeoutMs = -1)
		{
			//_ASSERT(false); // not tested
			using namespace std::chrono;

			std::lock_guard		lock(mMutex);
			AtomicStateGuard	stateGuard(mIsPendingOperation);
			bool				isSuccess = true;

			if(		(handle >= 0)
				&&	(handle < mCallbacks.size()) 
				&&	mCallbacks[handle].has_value()
				&&	mCallbackFutures[handle].valid())
			{
				if(timeoutMs > 0)
				{
					isSuccess = (mCallbackFutures[handle].wait_for(milliseconds(timeoutMs)) == std::future_status::ready);
					if(isSuccess)
					{
						try
						{
							mCallbackFutures[handle].get(); // prüfen, ob Exception in Callback aufgetreten ist
						}
						catch(const std::exception&)
						{
							if(!handleException)
							{
								throw;
							}
							isSuccess = false;
						}
					}
				}
				else
				{
					try
					{
						mCallbackFutures[handle].get(); // prüfen, ob Exception in Callback aufgetreten ist
					}
					catch(const std::exception&)
					{
						if(!handleException)
						{
							throw;
						}
						isSuccess = false;
					}
				}
			}
			return isSuccess;
		}

	private:
		using OptionalCallback = std::optional<CallbackType>;

		mutable std::mutex				mMutex;
		std::atomic_bool				mIsPendingOperation = false; // Merker, ob Schnittstelle für unbestimmte Zeit blockiert ist
		std::vector<OptionalCallback>	mCallbacks;
		std::vector<std::future<void>>	mCallbackFutures;
	};

} // namespace asentics::concurrent