#pragma once
#include <queue>
#include <optional>
#include <functional>
#include <atomic>
#include <limits>
#include <mutex>
#include <condition_variable>

namespace tiel::concurrent::container
{
	//_________________________________________________________________________________________________
	/// @brief	Threadsichere Queue, in der jede Methode blockierungsfrei implementiert ist.
	/// @remark	Diese Klasse sollte nur verwendet werden, wenn das Kopieren von <T> geringe Rechenzeit
	///			ben�tigt oder zumindest Race-Condition bei Push- und Pop-Operationen sehr selten
	///			auftreten, da sonst der zur Synchronistation verwendete interne SpinLock zu einer hohen
	///			CPU-Auslastung f�hren kann.
	///			Bei teuer zu kopierenden Typen ist es u.U. besser statt <T>, <std::shared_ptr<T>> bzw. 
	///			<std::unique_ptr<T>> als Vorlagentyp zu verwenden.
	/// @tparam T	Move-Konstruktor und Move-Zuweisungsoperator d�rfen nicht explizit gel�scht sein
	template <typename T>
	class LockFreeQueue final
	{
	public:
		///----------------------------------------------------------------------------------------------
		/// Copy-Konstruktor und Copy-Zuweisung nicht erlaubt
		LockFreeQueue(const LockFreeQueue&) = delete;
		LockFreeQueue& operator=(const LockFreeQueue&) = delete;
		///----------------------------------------------------------------------------------------------
		/// Destruktor
		~LockFreeQueue()
		{
			//_ASSERT(false); // not tested
			Close();
			Reset();
		}
		///----------------------------------------------------------------------------------------------
		/// @brief Default-Konstruktor
		LockFreeQueue()
			: mQueue()
		{}
		///----------------------------------------------------------------------------------------------
		/// @brief Konstruktor, mit dem optional die maximale Queue-Gr��e definiert werden kann
		/// @param maxSize		max. Anzahl Elemente, die gleichzeitig in der Queue gehalten werden.
		LockFreeQueue(std::size_t maxSize)
			: mQueue(), mMaxSize(maxSize)
		{}
		///----------------------------------------------------------------------------------------------
		/// @brief Move-Konstruktor
		/// @param mv_other [in, out]:		 mv_other ist anschlie�end leer und geschlossen
		LockFreeQueue(LockFreeQueue&& mv_other) noexcept
		{
			while(mv_other.mFlag.test_and_set(std::memory_order_acquire))
				;

			mQueue = std::move(mv_other.mQueue);
			mMaxSize = mv_other.mMaxSize;
			mIsEmpty = mv_other.mIsEmpty.load(std::memory_order_relaxed);
			mIsClosed = mv_other.mIsClosed.load(std::memory_order_relaxed);

			mv_other.mIsClosed.store(true, std::memory_order_release);
			mv_other.mIsEmpty.store(true, std::memory_order_release);
			mv_other.mFlag.clear(std::memory_order_release);
		}
		///----------------------------------------------------------------------------------------------
		/// @brief Move-Zuweisungsoperator
		/// @param mv_right [in, out]:		mv_other ist anschlie�end leer und geschlossen
		/// @return							diese Queue
		LockFreeQueue& operator=(LockFreeQueue&& mv_right) noexcept
		{
			if(&mv_right != this)
			{
				while(mFlag.test_and_set(std::memory_order_acquire))
					;
				while(mv_right.mFlag.test_and_set(std::memory_order_acquire))
					;

				mQueue = std::move(mv_right.mQueue);
				mMaxSize = mv_right.mMaxSize;
				mIsEmpty = mv_right.mIsEmpty.load(std::memory_order_relaxed);
				mIsClosed = mv_right.mIsClosed.load(std::memory_order_relaxed);

				mv_right.mIsClosed.store(true, std::memory_order_release);
				mv_right.mIsEmpty.store(true, std::memory_order_release);
				mv_right.mFlag.clear(std::memory_order_release);

				mFlag.clear(std::memory_order_release);
			}
			return *this;
		}
		///----------------------------------------------------------------------------------------------
		/// @brief	Entfernt alle Elemente aus der Queue.
		inline void Reset()
		{
			//_ASSERT(false); // not tested
			while(mFlag.test_and_set(std::memory_order_acquire))
				;

			while(!mQueue.empty())
			{
				mQueue.pop();
			}
			mIsEmpty.store(true, std::memory_order_release);
			mFlag.clear(std::memory_order_release);
		}
		///----------------------------------------------------------------------------------------------
		/// @brief Entfernt alle Elemente aus der Queue, f�r die die Filterfunktion true zur�ckgibt
		/// @param filter		Filterfunktion, die f�r alle zu entfernende Elemente true zur�ckgibt.
		/// @return				true, wenn mindestens ein Element aus der Queue entfernt wurde.
		bool RemoveByFilter(std::function<bool(const T& value)> filter)
		{
			_ASSERT(false); // not tested
			while(mFlag.test_and_set(std::memory_order_acquire))
				;
			const size_t numElements = mQueue.size();

			for(size_t i = 0; i < numElements; i++)
			{
				if(!filter(mQueue.front()))
				{
					if constexpr(std::is_move_assignable<T>::value)
					{
						mQueue.push(std::move(mQueue.front()));
					}
					else
					{
						mQueue.push(mQueue.front());
					}
				}
				mQueue.pop();
			}
			bool isRemoved = (mQueue.size() < numElements);
			mIsEmpty.store(mQueue.empty(), std::memory_order_release);
			mFlag.clear(std::memory_order_release);

			return isRemoved;
		}
		///----------------------------------------------------------------------------------------------
		/// @brief	Schlie�t die Queue, sodass keine weiteren Elemente mit TryPush() in die Queue 
		///			aufgenommen werden. Bereits in der Queue befindliche Elemente k�nnen noch mit
		///			TryPop entnommen werden.
		/// @remark	Eine einmal geschlossene Queue kann nicht wieder ge�ffnet werden.
		void Close()
		{
			//_ASSERT(false); // not tested
			if(!mIsClosed.load())
			{
				while(mFlag.test_and_set(std::memory_order_acquire))
					;
				mIsClosed.store(true, std::memory_order_release);
				mFlag.clear(std::memory_order_release);
			}
		}
		///----------------------------------------------------------------------------------------------
		/// @brief Pr�ft, ob die Queue geschlossen ist.
		/// @return			true, wenn die Queue geschlossen ist.
		[[nodiscard]] bool IsClosed() const
		{
			return mIsClosed.load(std::memory_order_relaxed);
		}
		///----------------------------------------------------------------------------------------------
		/// @brief	Pr�ft, ob die Queue voll ist.
		/// @return			true, wenn die Queue die maximale Anzahl Elemente enth�lt
		[[nodiscard]] bool IsFull() const
		{
			while(mFlag.test_and_set(std::memory_order_acquire))
				;

			auto size = mQueue.size();
			mFlag.clear(std::memory_order_release);
			return (size >= mMaxSize);
		}
		///----------------------------------------------------------------------------------------------
		/// @brief Pr�ft, ob die Queue leer ist.
		/// @return		true, wenn die Queue keine Elemente enth�lt.
		[[nodiscard]] bool IsEmpty() const
		{
			return mIsEmpty.load(std::memory_order_relaxed);
		}
		///----------------------------------------------------------------------------------------------
		/// @brief	Pr�ft, ob das erste Element der Queue (front), dem das reingereichte Pr�dikat erf�llt.
		/// @param predicate		Funktion, die true zur�ckgibt, wenn predicate(queue.front()) die
		///							Bedingung erf�llt.
		/// @return		true, wenn predicate(queue.front()) == true ist, sonst false.
		[[nodiscard]] bool IsFront(std::function<bool(const T&)> predicate) const
		{
			_ASSERT(false); // not tested
			if(!IsEmpty())
			{
				while(mFlag.test_and_set(std::memory_order_acquire))
					;

				if(!mQueue.empty())
				{
					bool isFullfilled = predicate(mQueue.front());
					mFlag.clear(std::memory_order_release);
					return isFullfilled;
				}
				mFlag.clear(std::memory_order_release);
			}
			return false;
		}
		///----------------------------------------------------------------------------------------------
		/// @brief Gibt die Anzahl der Queue-Elemente zur�ck.
		/// @return			Anzahl der Queue-Elemente
		[[nodiscard]] size_t Size() const
		{
			while(mFlag.test_and_set(std::memory_order_acquire))
				;

			auto size = mQueue.size();
			mFlag.clear(std::memory_order_release);
			return size;
		}
		///----------------------------------------------------------------------------------------------
		/// @brief Gibt die maximale g�ltige Anzahl Queue-Elemente zur�ck.
		/// @return			Maximale g�ltige Anzahl Queue-Elemente
		[[nodiscard]] size_t MaxSize() const
		{
			return mMaxSize;
		}
		///----------------------------------------------------------------------------------------------
		/// @brief	F�gt ein Element der Queue hinzu, sofern die Queue nicht geschlossen oder voll ist.
		/// @param value	Wert, der am Ende der Queue hinzugef�gt werden soll.
		/// @return			true, wenn das angegene Element der Queue hinzugef�gt werden konnte.
		[[nodiscard]] bool TryPush(const T& value)
		{
			if(mIsClosed.load(std::memory_order_relaxed))
			{
				return false;
			}
			while(mFlag.test_and_set(std::memory_order_acquire))
				;

			if(!mIsClosed.load(std::memory_order_acquire) && (mQueue.size() < mMaxSize))
			{
				mQueue.push(value);
				mIsEmpty.store(false, std::memory_order_release);
				mFlag.clear(std::memory_order_release);
				return true;
			}
			mFlag.clear(std::memory_order_release);
			return false;
		}
		///----------------------------------------------------------------------------------------------
		/// @brief	Verschiebt das �bergebene Element in die Queue, sofern die Queue nicht geschlossen 
		///			oder voll ist.
		/// @remark	Wenn das Element in die Queue verschoben werden konnt, ist "value"
		/// @param value [in, out]		Element, das in die Queue verschoben werden soll. Wenn die Methode
		///								mit true zur�ckkehrt, ist "value" anschlie�end in einem g�ltigen
		///								aber unbestimmten Zustand.
		/// @return						true, wenn das Element in die Queue verschoben werden konnte.
		[[nodiscard]] bool TryPush(T&& mv_value)
		{
			if(mIsClosed.load(std::memory_order_relaxed))
			{
				return false;
			}
			while(mFlag.test_and_set(std::memory_order_acquire))
				;

			if(!mIsClosed.load(std::memory_order_acquire) && (mQueue.size() < mMaxSize))
			{
				if constexpr(std::is_move_assignable<T>::value)
				{
					mQueue.push(std::forward<T>(mv_value));
				}
				else
				{
					mQueue.push(mv_value);
				}
				mIsEmpty.store(false, std::memory_order_release);
				mFlag.clear(std::memory_order_release);
				return true;
			}
			mFlag.clear(std::memory_order_release);
			return false;
		}
		///----------------------------------------------------------------------------------------------
		/// @brief Entfernt das erste Element aus der Queue und gibt dieses zur�ck.
		/// @return		wenn die Queue leer ist, h�lt das zur�ckgegebene std::optional<T> keinen Wert,
		///				sont wird der Wert des ersten Elements der Queue zur�ckgegeben.
		std::optional<T> TryPop()
		{
			// stellt sicher, dass bei einer leeren Queue TryPush bevorzugt wird
			if(mIsEmpty.load(std::memory_order_acquire))
			{
				return {};
			}

			while(mFlag.test_and_set(std::memory_order_acquire))
				;

			if(!mQueue.empty())
			{
				T retval = std::move(mQueue.front());
				mQueue.pop();
				if(mQueue.empty())
				{
					mIsEmpty.store(true, std::memory_order_release);
				}
				mFlag.clear(std::memory_order_release);
				return std::move(retval);
			}
			mFlag.clear(std::memory_order_release);
			return {};
		}

		private:
		mutable std::atomic_flag	mFlag		= ATOMIC_FLAG_INIT;
		std::atomic_bool			mIsEmpty	= true;
		std::atomic_bool			mIsClosed	= false;
		size_t						mMaxSize	= (std::numeric_limits<size_t>::max)();
		std::queue<T>				mQueue;

	}; // class LockFreeQueue
	
	///_________________________________________________________________________________________________
	/// @brief	Threadsichere Queue mit blockierender Schnittstelle.
	/// @remark	Die korrekte Behandlung einer gr��enbegrenzten Queue f�hrt zu deutlichen schlechteren
	///			Performance, weshalb die Gr��e der BlockingQueue (im Gegensatz zur LockfreeQueue) nicht 
	///			vorgegeben werden kann.
	/// @tparam T	Move-Konstruktor und Move-Zuweisungsoperator d�rfen nicht explizit gel�scht sein
	template <typename T>
	class BlockingQueue final
	{
	public:
		///----------------------------------------------------------------------------------------------
		/// Copy-Konstruktor und Copy-Zuweisung nicht erlaubt
		BlockingQueue(const BlockingQueue&)				= delete;
		BlockingQueue& operator=(const BlockingQueue&)	= delete;
		///----------------------------------------------------------------------------------------------
		/// Destruktor
		~BlockingQueue()
		{
			//_ASSERT(false); // not tested
			Reset(false);
		}
		///----------------------------------------------------------------------------------------------
		/// @brief Konstruktor, mit dem optional die maximale Queue-Gr��e definiert werden kann
		/// @param maxSize		max. Anzahl Elemente, die gleichzeitig in der Queue gehalten werden.
		BlockingQueue()
			: mQueue()
		{
			//_ASSERT(false); // not tested
		}
		///----------------------------------------------------------------------------------------------
		/// @brief Move-Konstruktor
		/// @param mv_other [in, out]:		 mv_other ist anschlie�end leer und geschlossen
		BlockingQueue(BlockingQueue&& mv_other) noexcept
		{
			//_ASSERT(false); // not tested
			std::lock_guard lock(mv_other.mMutex);
			mQueue = std::move(mv_other.mQueue);
			mQueueSize.store(mv_other.mQueueSize);
			mIsClosed.store(mv_other.mIsClosed);

			mv_other.mIsClosed	= true;
			mv_other.mQueueSize = 0;
		}
		///----------------------------------------------------------------------------------------------
		/// @brief Move-Zuweisungsoperator
		/// @param mv_right [in, out]:		mv_other ist anschlie�end leer und geschlossen
		/// @return							diese Queue
		BlockingQueue& operator=(BlockingQueue&& mv_right) noexcept
		{
			//_ASSERT(false); // not tested
			if(&mv_right != this)
			{
				std::scoped_lock lock(mMutex, mv_right.mMutex);
				mQueue = std::move(mv_right.mQueue);
				mQueueSize.store(mv_right.mQueueSize);
				mIsClosed.store(mv_right.mIsClosed);

				mv_right.mIsClosed = true;
				mv_right.mQueueSize = 0;
			}
			return *this;
		}
		///----------------------------------------------------------------------------------------------
		/// @brief	Schlie�t die Queue, entfernt alle Elemente aus der Queue und �ffnet diese optional
		///			wieder, sofern diese zuvor offen war.
		/// @remark	Threads, die durch einen Pop()-Aufruf blockiert sind, kehren mit einem leeren
		///			optional<T> zur�ck. Mittels IsClosed() k�nnen diese Entscheiden, ob sie die
		///			Thread-Loop verlassen
		/// @param reopen [in]:		true, wenn die Queue nach dem Leeren wieder ge�ffnet werden soll,
		///							sofern diese zuvor offen war.
		inline void Reset(bool reopen)
		{
			//_ASSERT(false); // not tested
			{
				std::lock_guard lock(mMutex);

				if(!reopen)
				{
					mIsClosed.store(true, std::memory_order_release);
				}
				mQueueSize.store(0, std::memory_order_release);
				while(!mQueue.empty())
				{
					mQueue.pop();
				}
			}
			// blockierten Threads die M�glichkeit geben, auf Reset() zu reagieren
			mCV.notify_all();
		}
		///----------------------------------------------------------------------------------------------
		/// @brief Entfernt alle Elemente aus der Queue, f�r die die Filterfunktion true zur�ckgibt
		/// @param filter		Filterfunktion, die f�r alle zu entfernende Elemente true zur�ckgibt.
		/// @return				true, wenn mindestens ein Element aus der Queue entfernt wurde.
		bool RemoveByFilter(std::function<bool(const T& value)> filter)
		{
			_ASSERT(false); // not tested
			std::lock_guard lock(mMutex);
			const size_t numElements = mQueue.size();

			for(size_t i = 0; i < numElements; i++)
			{
				if(!filter(mQueue.front()))
				{
					if constexpr(std::is_move_assignable<T>::value)
					{
						mQueue.push(std::move(mQueue.front()));
					}
					else
					{
						mQueue.push(mQueue.front());
					}
				}
				mQueue.pop();
			}
			bool isRemoved = (mQueue.size() < numElements);
			mQueueSize.store(mQueue.size(), std::memory_order_release);
			// notify darf nur aufgerufen werden, wenn das mCV.wait-Pr�dikat true zur�ck gibt, sonst kann es passieren,
			//  dass w�hrend des Ausf�hrens eines False-Pr�dikats ein weiterer notify-Aufruf verschluckt wird,
			//  dessen Pr�dikat true zur�ck geben w�rde. Pop() w�rde weiterhin ungewollt blockieren.
			if(!mQueue.empty())
			{
				// blockierten Threads die M�glichkeit geben, auf Reset() zu reagieren
				if(mIsClosed.load(std::memory_order_acquire))
					mCV.notify_all();
				else
					mCV.notify_one();
			}

			return isRemoved;
		}
		///----------------------------------------------------------------------------------------------
		/// @brief	Schlie�t die Queue, so dass keine weiteren Elemente mit TryPush in die Queue 
		///			aufgenommen werden. Bereits in der Queue befindliche Elemente k�nnen noch mit
		///			TryPop entnommen werden.
		/// @remark	Eine einmal geschlossene Queue kann nicht wieder ge�ffnet werden.
		inline void Close()
		{
			//_ASSERT(false); // not tested
			{
				std::lock_guard lock(mMutex);
				mIsClosed.store(true, std::memory_order_release);
			}
			// blockierten Threads die m�glichkeit geben, auf Close() zu reagieren
			mCV.notify_all();
		}
		///----------------------------------------------------------------------------------------------
		/// @brief	Pr�ft, ob die Queue geschlossen ist. 
		/// @remark	Eine einmal geschlossene Queue kann nicht wieder ge�ffnet werden.
		/// @return			true, wenn die Queue geschlossen ist.
		[[nodiscard]] inline bool IsClosed() const
		{
			return mIsClosed.load(std::memory_order_relaxed);
		}
		///----------------------------------------------------------------------------------------------
		/// @brief Pr�ft, ob die Queue leer ist.
		/// @return		true, wenn die Queue keine Elemente enth�lt.
		[[nodiscard]] inline bool IsEmpty() const
		{
			return (mQueueSize.load(std::memory_order_relaxed) == 0);
		}
		///----------------------------------------------------------------------------------------------
		/// @brief	Pr�ft, ob das erste Element der Queue (front), dem das reingereichte Pr�dikat erf�llt.
		/// @param predicate		Funktion, die true zur�ckgibt, wenn predicate(queue.front()) die
		///							Bedingung erf�llt.
		/// @return		true, wenn predicate(queue.front()) == true ist, sonst false.
		[[nodiscard]] bool IsFront(std::function<bool(const T&)> predicate) const
		{
			//_ASSERT(false); // not tested
			if(!IsEmpty())
			{
				std::lock_guard lock(mMutex);
				if(!mQueue.empty())
				{
					return predicate(mQueue.front());
				}
			}
			return false;
		}
		///----------------------------------------------------------------------------------------------
		/// @brief	Gibt die Anzahl der Queue-Elemente zur�ck.
		/// @remark	Diese Methode ist blockierungsfrei, gibt aber u.U. nicht den aktuellen Stand der
		///			Queue-Gr��e wider und sollte daher nicht innerhalb dieser Klasse verwendet werden.
		/// @return			Anzahl der Queue-Elemente zum aktuellen Zeitpunkt 
		///					(M�glicherweise wird gerade eine Push- oder Pop-Operation durchgef�hrt)
		[[nodiscard]] inline size_t Size() const
		{
			return mQueueSize.load(std::memory_order_relaxed);
		}
		///----------------------------------------------------------------------------------------------
		/// @brief	F�gt ein Element an das Ende der Queue an, sofern die Queue offen und nicht voll ist.
		/// @remark	Push() kehrt sofort zur�ck, wenn die Queue geschlossen ist. Wenn die Queue voll ist,
		///			blockiert der Aufruf, bis die Queue nicht mehr voll ist oder die Queue geschlossen wird.
		/// @param value [in]:	Element, welches am Ende der Queue eingef�gt wird, sofern die Queue
		///						nicht geschlossen ist.
		/// @return				true, wenn das Element an das Ende der Queue angef�gt wurde.
		[[nodiscard]] bool Push(const T& value)
		{
			//_ASSERT(false); // not tested
			bool isPushed = false;
			{
				std::lock_guard lock(mMutex);

				if(!mIsClosed.load(std::memory_order_acquire))
				{
					mQueue.push(value);
					mQueueSize.store(mQueue.size(), std::memory_order_release);
					isPushed = true;
					mCV.notify_one();
				}
				else
				{
					mCV.notify_all();
				}
			}
			return isPushed;
		}
		///----------------------------------------------------------------------------------------------
		/// @brief	F�gt ein Element an das Ende der Queue an, sofern die Queue offen und nicht voll ist.
		/// @remark	Push() kehrt sofort zur�ck, wenn die Queue geschlossen ist.
		/// @param mv_value [in]:	Element, welches am Ende der Queue eingef�gt wird, sofern die Queue
		///							nicht geschlossen ist.
		/// @return					true, wenn das Element an das Ende der Queue angef�gt wurde.
		[[nodiscard]] bool Push(T&& mv_value)
		{
			//_ASSERT(false); // not tested
			bool isPushed = false;
			{
				std::lock_guard lock(mMutex);

				if(!mIsClosed.load(std::memory_order_acquire))
				{
					if constexpr(std::is_move_assignable<T>::value)
					{
						mQueue.push(std::forward<T>(mv_value));
					}
					else
					{
						mQueue.push(mv_value);
					}
					mQueueSize.store(mQueue.size(), std::memory_order_release);
					isPushed = true;
					mCV.notify_one();
				}
				else
				{
					mCV.notify_all();
				}
			}
			return isPushed;
		}
		///----------------------------------------------------------------------------------------------
		/// @brief	Entnimmt das Element am Anfang der Queue und gibt dieses zur�ck.
		/// @remark	Der aufruf blockiert, wenn die Queue offen und leer ist, bis ein Element mit Push()
		///			hinzugef�gt wurde oder die Queue leer und geschlossen ist.
		/// @return			Element, welches am Ende der Queue entnommen wurde, bzw. ein leeres Element,
		///					wenn die Queue leer und geschlossen ist.
		std::optional<T> Pop()
		{
			//_ASSERT(false); // not tested
			std::unique_lock ulock(mMutex);
			mCV.wait(ulock, [this]() 
				{ 
					return (mQueueSize.load(std::memory_order_acquire) != 0) || mIsClosed.load(std::memory_order_acquire);
				});

			// mMutex wird ab hier bis zum Verlassen gehalten
			if(!mQueue.empty())
			{
				if constexpr(std::is_move_assignable<T>::value)
				{
					std::optional<T> optValue{ std::move(mQueue.front()) };
					mQueue.pop();
					mQueueSize.store(mQueue.size(), std::memory_order_release);
					// notify darf nur aufgerufen werden, wenn das mCV.wait-Pr�dikat true zur�ck gibt, sonst kann es passieren,
					//  dass w�hrend des Ausf�hrens eines False-Pr�dikats ein weiterer notify-Aufruf verschluckt wird,
					//  dessen Pr�dikat true zur�ck geben w�rde. Pop() w�rde weiterhin ungewollt blockieren.
					if(!mQueue.empty())
					{
						if(mIsClosed.load(std::memory_order_acquire))
							mCV.notify_all();
						else
							mCV.notify_one();
					}
					return std::move(optValue);
				}
				else
				{
					std::optional<T> optValue{ mQueue.front() };
					mQueue.pop();
					mQueueSize.store(mQueue.size(), std::memory_order_release);
					// notify darf nur aufgerufen werden, wenn das mCV.wait-Pr�dikat true zur�ck gibt, sonst kann es passieren,
					//  dass w�hrend des Ausf�hrens eines False-Pr�dikats ein weiterer notify-Aufruf verschluckt wird,
					//  dessen Pr�dikat true zur�ck geben w�rde. Pop() w�rde weiterhin ungewollt blockieren.
					if(!mQueue.empty())
					{
						if(mIsClosed.load(std::memory_order_acquire))
							mCV.notify_all();
						else
							mCV.notify_one();
					}
					return optValue;
				}
			}
			return {};
		}
		///----------------------------------------------------------------------------------------------
		/// @brief	Entnimmt das Element am Anfang der Queue und gibt dieses zur�ck.
		/// @remark	Der aufruf blockiert, wenn die Queue offen und leer ist, bis ein Element mit Push()
		///			hinzugef�gt wurde oder die Queue leer und geschlossen ist.
		/// @param waitDurationMS [in]:	max. Zeit in Millisekunden, die auf die Entnahme eines Elements
		///								gewartet wird.
		///								< 0, wenn ohne Zeitbegrenzung auf die Entnahme eines Elemements
		///								gewartet werden solloder bis die Queue geschlossen wurde.
		/// @return						Element, welches am Ende der Queue entnommen wurde, bzw. ein
		///								leeres Element, wenn die Queue geschlossen ist oder innerhalb
		///								der angegebenen Zeitspanne kein Element entnommen werden konnte.
		std::optional<T> Pop(int waitDurationMS)
		{
			//_ASSERT(false); // not tested
			if(waitDurationMS < 0)
			{
				return Pop();
			}
			const std::chrono::milliseconds waitMS(waitDurationMS);
			std::unique_lock				ulock(mMutex);

			if(!mCV.wait_for(ulock, waitMS, [this]()
				{
					return (mQueueSize.load(std::memory_order_acquire) != 0) || mIsClosed.load(std::memory_order_acquire);
				}))
			{
				// Timeout
				return {};
			}

			// mMutex wird ab hier bis zum Verlassen gehalten
			if(!mQueue.empty())
			{
				if constexpr(std::is_move_assignable<T>::value)
				{
					std::optional<T> optValue = std::move(mQueue.front());
					mQueue.pop();
					mQueueSize.store(mQueue.size(), std::memory_order_release);
					// notify darf nur aufgerufen werden, wenn das mCV.wait-Pr�dikat true zur�ck gibt, sonst kann es passieren,
					//  dass w�hrend des Ausf�hrens eines False-Pr�dikats ein weiterer notify-Aufruf verschluckt wird,
					//  dessen Pr�dikat true zur�ck geben w�rde. Pop() w�rde weiterhin ungewollt blockieren.
					if(!mQueue.empty())
					{
						if(mIsClosed.load(std::memory_order_relaxed))
							mCV.notify_all();
						else
							mCV.notify_one();
					}
					return std::move(optValue);
				}
				else
				{
					std::optional<T> optValue = mQueue.front();
					mQueue.pop();
					mQueueSize.store(mQueue.size(), std::memory_order_release);
					// notify darf nur aufgerufen werden, wenn das mCV.wait-Pr�dikat true zur�ck gibt, sonst kann es passieren,
					//  dass w�hrend des Ausf�hrens eines False-Pr�dikats ein weiterer notify-Aufruf verschluckt wird,
					//  dessen Pr�dikat true zur�ck geben w�rde. Pop() w�rde weiterhin ungewollt blockieren.
					if(!mQueue.empty())
					{
						if(mIsClosed.load(std::memory_order_relaxed))
							mCV.notify_all();
						else
							mCV.notify_one();
					}
					return optValue;
				}
			}
			return {};
		}

	private:
		std::condition_variable	mCV;
		mutable std::mutex		mMutex;
		std::queue<T>			mQueue;
		std::atomic_bool		mIsClosed			= false;
		std::atomic_size_t		mQueueSize			= 0;
	}; // class BlockingQueue

} // namespace asentics::concurrent::container
