#pragma once
#include <ctime>
#include <chrono>
#include <string>
#include <string_view>

namespace tiel::timer
{
	//_________________________________________________________________________________________________
	// Einfacher std::chrono-basierter Timer.
	// Die Startzeit wird im Konstruktor erstmalig gesetzt.
	class SimpleTimer
	{
		std::chrono::steady_clock::time_point mStartSteadyTP{};
		std::chrono::steady_clock::time_point mStopSteadyTP{};
		std::chrono::steady_clock::time_point mSteadyTPMidnightUTC	= std::chrono::steady_clock::now();
		std::chrono::system_clock::time_point mEpochTPMidnightUTC	= std::chrono::system_clock::now();

	public:
		//---------------------------------------------------------------------------------------------
		SimpleTimer()
		{
			using namespace std::chrono;
			using namespace std::chrono_literals;

			auto dayDuration		 = mEpochTPMidnightUTC.time_since_epoch()%24h;
			mEpochTPMidnightUTC		-= std::chrono::duration_cast<microseconds>(dayDuration);
			mSteadyTPMidnightUTC	-= std::chrono::duration_cast<microseconds>(dayDuration);
			mStartSteadyTP			 = std::chrono::steady_clock::now();;
			mStopSteadyTP			 = mStartSteadyTP;
		}
		//---------------------------------------------------------------------------------------------
		/// Copy-Konstruktor
		SimpleTimer(const SimpleTimer& other)
			:	mStartSteadyTP(other.mStartSteadyTP),
				mStopSteadyTP(other.mStopSteadyTP),
				mSteadyTPMidnightUTC(other.mSteadyTPMidnightUTC),
				mEpochTPMidnightUTC(other.mEpochTPMidnightUTC)
		{ }
		//---------------------------------------------------------------------------------------------
		/// Copy-Assignment
		SimpleTimer& operator=(const SimpleTimer& rhs)
		{
			mStartSteadyTP			= rhs.mStartSteadyTP;
			mStopSteadyTP			= rhs.mStopSteadyTP;
			mSteadyTPMidnightUTC	= rhs.mSteadyTPMidnightUTC;
			mEpochTPMidnightUTC		= rhs.mEpochTPMidnightUTC;
			return *this;
		}
		//---------------------------------------------------------------------------------------------
		// setzt die Start-Zeit
		void Start()
		{
			mStartSteadyTP = std::chrono::steady_clock::now();
			mStopSteadyTP = mStartSteadyTP;
		}
		//---------------------------------------------------------------------------------------------
		// setzt die Stop-Zeit
		void Stop()
		{
			mStopSteadyTP = std::chrono::steady_clock::now();
		}
		//---------------------------------------------------------------------------------------------
		// Gibt die, mit xxxStop() gemessene Zeit in Mikrosekunden zurück
		uint64_t llGetMeasuredTimeUs() const
		{
			return std::chrono::duration_cast<std::chrono::microseconds>(mStopSteadyTP - mStartSteadyTP).count();
		}
		//---------------------------------------------------------------------------------------------
		// Gibt die, mit xxxStop() gemessene Zeit in Mikrosekunden zurück
		uint32_t lGetMeasuredTimeUs() const
		{
			return static_cast<int32_t>(std::chrono::duration_cast<std::chrono::microseconds>(mStopSteadyTP - mStartSteadyTP).count());
		}
		//---------------------------------------------------------------------------------------------
		// Gibt die, mit xxxStop() gemessene Zeit in Millisekunden zurück
		uint64_t llGetMeasuredTimeMs() const
		{
			return std::chrono::duration_cast<std::chrono::milliseconds>(mStopSteadyTP - mStartSteadyTP).count();
		}
		//---------------------------------------------------------------------------------------------
		// Gibt die, mit xxxStop() gemessene Zeit in Millisekunden zurück
		uint32_t lGetMeasuredTimeMs() const
		{
			return static_cast<int32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(mStopSteadyTP - mStartSteadyTP).count());
		}
		//---------------------------------------------------------------------------------------------
		// Gibt die, mit xxxStop() gemessene Zeit mit Nachkommastellen in Millisekunden zurück
		double dGetMeasuredTimeMs() const
		{
			return static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(mStopSteadyTP - mStartSteadyTP).count()) / 1000.0;
		}
		//---------------------------------------------------------------------------------------------
		// setzt die Stop-Zeit und gibt die abgelaufene Zeit seit Start() in Millisekunden als double zurück
		double dStopMs()
		{
			mStopSteadyTP = std::chrono::steady_clock::now();
			return static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(mStopSteadyTP - mStartSteadyTP).count()) / 1000.0;
		}
		//---------------------------------------------------------------------------------------------
		// setzt die Stop-Zeit und gibt die abgelaufene Zeit in Millisekunden zurück
		int32_t lStopMs()
		{
			mStopSteadyTP = std::chrono::steady_clock::now();
			return static_cast<int32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(mStopSteadyTP - mStartSteadyTP).count());
		}
		//---------------------------------------------------------------------------------------------
		// setzt die Stop-Zeit und gibt die abgelaufene Zeit in Millisekunden zurück
		int64_t llStopMs()
		{
			mStopSteadyTP = std::chrono::steady_clock::now();
			return std::chrono::duration_cast<std::chrono::milliseconds>(mStopSteadyTP - mStartSteadyTP).count();
		}
		//---------------------------------------------------------------------------------------------
		// setzt die Stop-Zeit und gibt die abgelaufene Zeit seit Start() in Mikrosekunden zurück
		int32_t lStopUs()
		{
			mStopSteadyTP = std::chrono::steady_clock::now();
			return static_cast<int32_t>(std::chrono::duration_cast<std::chrono::microseconds>(mStopSteadyTP - mStartSteadyTP).count());
		}
		//---------------------------------------------------------------------------------------------
		// setzt die Stop-Zeit und gibt die abgelaufene Zeit seit Start() in Mikrosekunden als int64_t zurück
		int64_t llStopUs()
		{
			mStopSteadyTP = std::chrono::steady_clock::now();
			return std::chrono::duration_cast<std::chrono::microseconds>(mStopSteadyTP - mStartSteadyTP).count();
		}
		//---------------------------------------------------------------------------------------------
		// setzt die Stop-Zeit und gibt die abgelaufene Zeit seit Start() in Millisekunden als double zurück
		std::string StopMs()
		{
			return std::to_string(dStopMs());
		}
		//---------------------------------------------------------------------------------------------
		// gibt die abgelaufene Zeit seit Start() in Millisekunden als double zurück
		double dElapseMs() const
		{
			return static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - mStartSteadyTP).count()) / 1000.0;
		}
		//---------------------------------------------------------------------------------------------
		// gibt die abgelaufene Zeit in Millisekunden zurück
		int32_t lElapsedMs() const
		{
			return static_cast<int32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - mStartSteadyTP).count());
		}
		//---------------------------------------------------------------------------------------------
		// gibt die abgelaufene Zeit in Millisekunden zurück
		int64_t llElapsedMs() const
		{
			return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - mStartSteadyTP).count();
		}
		//---------------------------------------------------------------------------------------------
		// gibt die abgelaufene Zeit seit Start() in Mikrosekunden zurück
		int32_t lElapseUs() const
		{
			return static_cast<int32_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - mStartSteadyTP).count());
		}
		//---------------------------------------------------------------------------------------------
		// gibt die abgelaufene Zeit seit Start() in Mikrosekunden als int64_t zurück
		int64_t llElapseUs() const
		{
			return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - mStartSteadyTP).count();
		}
		//---------------------------------------------------------------------------------------------
		// gibt die abgelaufene Zeit seit Start() in Millisekunden mit Nachkommastellen als string zurück
		std::string ElapsedMs() const
		{
			return std::to_string(dElapseMs());
		}
		//---------------------------------------------------------------------------------------------
		/// @brief	Gibt den Zeitpunkt seit dem 1.1.1970 (UTC) zurück. Die Messung basiert jedoch auf 
		///			auf der monotonic std::chrono::steady_clock.
		std::chrono::system_clock::time_point TimePointUTC() const
		{
			using namespace std::chrono;
			return mEpochTPMidnightUTC + duration_cast<microseconds>(steady_clock::now()-mSteadyTPMidnightUTC);
		}
		//---------------------------------------------------------------------------------------------
		/// @brief	Gibt die verstrichene Zeit seit Mitternacht (UTC) des Erzeugungsdatums
		///			dieser Instanz zurück
		std::chrono::steady_clock::duration DurationSinceCreationMidnight() const
		{
			return (std::chrono::steady_clock::now()-mSteadyTPMidnightUTC);
		}
		//---------------------------------------------------------------------------------------------
		/// @brief	Gibt die verstrichene Zeit seit Mitternacht (UTC) des Erzeugungsdatums
		///			dieser Instanz zurück
		/// @param timepoint [in]		Zeitpunkt, für die die Zeitspanne bestimmt werden soll
		std::chrono::steady_clock::duration DurationSinceCreationMidnight(std::chrono::steady_clock::time_point timepoint) const
		{
			return (timepoint-mSteadyTPMidnightUTC);
		}
		//---------------------------------------------------------------------------------------------
		// gibt die bisher verstrichene Zeit in Millisekunden auf der Konsole aus
		void PrintElapsedTime(std::string_view _Prefix) const
		{
			printf("%s: %6.3f ms\n", _Prefix.data(), static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - mStartSteadyTP).count()) / 1000.0);
		}
		//---------------------------------------------------------------------------------------------
		// gibt die gestoppte Zeit in Millisekunden auf der Konsole aus
		void PrintTimeMeasurement(std::string_view _Prefix) const
		{
			printf("%s: %6.3f ms\n", _Prefix.data(), static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(mStopSteadyTP - mStartSteadyTP).count()) / 1000.0);
		}
	};
} // namespace asentics::timer