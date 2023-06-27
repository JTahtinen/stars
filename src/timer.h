#pragma once
#include <chrono>

typedef std::chrono::steady_clock::time_point TimePoint;

struct Timer
{
    TimePoint startTime;
    TimePoint updatedTime;
    TimePoint lastTime;

    inline void start()
	{
		startTime = std::chrono::high_resolution_clock::now();
		updatedTime = startTime;
		lastTime = startTime;
	}
	inline unsigned int getElapsedInMillis() const
	{
        TimePoint currentTime = std::chrono::high_resolution_clock::now();

        unsigned int result =  (unsigned int)std::chrono::duration_cast<std::chrono::milliseconds>
            (currentTime - startTime).count();
        return result;
	}
    inline unsigned int getElapsedLastUpdateMillis() const
    {
        unsigned int result =  (unsigned int)std::chrono::duration_cast<std::chrono::milliseconds>
            (updatedTime - startTime).count();
        return result;
    }
	inline unsigned int getMillisBetweenUpdates() const
	{
		unsigned int result =  (unsigned int)std::chrono::duration_cast<std::chrono::milliseconds>
            (updatedTime - lastTime).count();
        return result;
    }

    inline unsigned int getMillisSinceLastUpdate()
    {
        TimePoint currentTime = std::chrono::high_resolution_clock::now();

        unsigned int result =  (unsigned int)std::chrono::duration_cast<std::chrono::milliseconds>
            (currentTime - updatedTime).count();
        return result;        
    }
	inline void update()
	{
		lastTime = updatedTime;
	    updatedTime = std::chrono::high_resolution_clock::now();
	}
};
